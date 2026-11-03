#include "assetlist.h"

#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QResizeEvent>
#include <QSet>

#include "database.h"
#include "icons.h"
#include "theme.h"
#include "storage.h"

namespace {

constexpr int kThumbColumn = 0;
constexpr int kDateAddedColumn = 8;
constexpr int kThumbSize = 40;
constexpr int kIdRole = Qt::UserRole;
constexpr int kSortRole = Qt::UserRole + 1;

// Sorts on kSortRole when set, so "$20.00" comes before "$100.00".
class SortableItem : public QTableWidgetItem {
public:
    using QTableWidgetItem::QTableWidgetItem;
    bool operator<(const QTableWidgetItem &other) const override {
        const QVariant a = data(kSortRole), b = other.data(kSortRole);
        if (a.isValid() && b.isValid()) return a.toDouble() < b.toDouble();
        return QTableWidgetItem::operator<(other);
    }
};

} // namespace

AssetList::AssetList(Database *db, QWidget *parent)
    : QTableWidget(0, 9, parent)
    , m_db(db)
{
    setHorizontalHeaderLabels({"", "ID", "Name", "Category", "Purchased", "Purchase Price",
                               "Current Value", "Receipt", "Added"});
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setAlternatingRowColors(true);
    setShowGrid(false);
    setWordWrap(false);
    setFrameShape(QFrame::NoFrame);
    setIconSize(QSize(kThumbSize, kThumbSize));
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(52);
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    horizontalHeader()->setHighlightSections(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &AssetList::showContextMenu);

    QHeaderView *header = horizontalHeader();
    // Name gets a fixed share the user can drag; the last column takes what is left.
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(kThumbColumn, QHeaderView::Fixed);
    setColumnWidth(kThumbColumn, 56);
    header->setSectionResizeMode(2, QHeaderView::Interactive);
    setColumnWidth(2, 230);
    header->setSortIndicatorShown(true);
    header->setSortIndicator(kDateAddedColumn, Qt::DescendingOrder);
    setSortingEnabled(true);

    m_emptyLabel = new QLabel("No assets yet. Click Add Asset to get started.", viewport());
    m_emptyLabel->setObjectName("emptyState");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->hide();

    connect(this, &QTableWidget::itemSelectionChanged, this, &AssetList::onSelectionChanged);
}

void AssetList::resizeEvent(QResizeEvent *event) {
    QTableWidget::resizeEvent(event);
    m_emptyLabel->resize(viewport()->size());
}

void AssetList::loadAssets(const QVector<Asset> &assets) {
    m_assets = assets;
    setSortingEnabled(false);
    setRowCount(0);
    setRowCount(assets.size());
    for (int row = 0; row < assets.size(); ++row)
        populateRow(row, assets[row]);
    setSortingEnabled(true);

    m_emptyLabel->setVisible(assets.isEmpty());
    if (assets.isEmpty()) {
        m_emptyLabel->resize(viewport()->size());
        m_emptyLabel->raise();
    }
}

void AssetList::populateRow(int row, const Asset &asset) {
    auto *thumb = new SortableItem;
    thumb->setData(kIdRole, asset.id);
    thumb->setTextAlignment(Qt::AlignCenter);

    QPixmap pixmap;
    const QVector<AssetFile> files = m_db->getAssetFiles(asset.id);
    for (const AssetFile &file : files) {
        if (file.fileType != "image") continue;
        pixmap = Storage::thumbnail(Storage::storedPath(asset, file), kThumbSize, file.encrypted);
        break;
    }
    if (pixmap.isNull()) pixmap = Icons::photo(QColor("#B4BDC7"), 22).pixmap(22, 22);
    thumb->setData(Qt::DecorationRole, pixmap);
    setItem(row, kThumbColumn, thumb);

    const QStringList values = {
        asset.id,
        asset.name,
        asset.category.isEmpty() ? QStringLiteral("—") : asset.category,
        asset.datePurchase.isEmpty() ? QStringLiteral("—") : asset.datePurchase,
        moneyDisplay(asset.valueEstimate),
        moneyDisplay(asset.currentValue),
        asset.hasReceipt ? "Yes" : "—",
        asset.dateAdded.left(10),
    };
    for (int i = 0; i < values.size(); ++i) {
        const int column = i + 1;
        auto *item = new SortableItem(values[i]);
        item->setData(kIdRole, asset.id);
        if (column == 5 || column == 6) {
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            item->setData(kSortRole, (column == 5 ? asset.valueEstimate : asset.currentValue)
                                         .value_or(-1));
        }
        setItem(row, column, item);
    }
}

QVector<Asset> AssetList::selectedAssets() const {
    QSet<QString> seen;
    QVector<Asset> result;
    const QList<QTableWidgetItem *> items = selectedItems();
    for (const QTableWidgetItem *item : items) {
        const QString id = item->data(kIdRole).toString();
        if (id.isEmpty() || seen.contains(id)) continue;
        seen.insert(id);
        for (const Asset &asset : m_assets) {
            if (asset.id == id) {
                result.append(asset);
                break;
            }
        }
    }
    return result;
}

std::optional<Asset> AssetList::selectedAsset() const {
    const QVector<Asset> assets = selectedAssets();
    if (assets.isEmpty()) return std::nullopt;
    return assets.first();
}

void AssetList::onSelectionChanged() {
    const QVector<Asset> assets = selectedAssets();
    if (assets.size() == 1) emit assetSelected(assets.first());
    else if (assets.isEmpty()) emit selectionCleared();
}

void AssetList::showContextMenu(const QPoint &pos) {
    const QVector<Asset> assets = selectedAssets();
    if (assets.isEmpty()) return;

    QMenu menu(this);
    if (assets.size() == 1) {
        menu.addAction("Edit Asset", this, [this, assets] { emit editRequested(assets.first()); });
        menu.addAction("Duplicate Asset", this,
                       [this, assets] { emit duplicateRequested(assets.first()); });
        menu.addSeparator();
    }
    menu.addAction(QString("Delete %1 Asset%2").arg(assets.size()).arg(assets.size() > 1 ? "s" : ""),
                   this, [this, assets] { emit deleteRequested(assets); });
    menu.exec(viewport()->mapToGlobal(pos));
}
