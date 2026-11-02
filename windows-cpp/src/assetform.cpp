#include "assetform.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "config.h"
#include "database.h"
#include "storage.h"

namespace {

constexpr double kMaxValue = 9999999;

QWidget *rowOf(std::initializer_list<QWidget *> widgets, bool stretch = false) {
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    for (QWidget *widget : widgets)
        layout->addWidget(widget);
    if (stretch) layout->addStretch();
    return row;
}

QLabel *sectionLabel(const QString &text) {
    auto *label = new QLabel(text);
    label->setStyleSheet("font-weight: bold; margin-top: 8px;");
    return label;
}

QDoubleSpinBox *moneyBox() {
    auto *box = new QDoubleSpinBox;
    box->setPrefix("$ ");
    box->setRange(0, kMaxValue);
    box->setDecimals(2);
    return box;
}

// Fills a box from an existing value and ties it to its "Unknown" checkbox.
void bindValue(QDoubleSpinBox *box, QCheckBox *unknown, std::optional<double> value) {
    if (value) box->setValue(*value);
    unknown->setChecked(!value);
    box->setDisabled(!value);
    QObject::connect(unknown, &QCheckBox::toggled, box, &QWidget::setDisabled);
}

} // namespace

AssetForm::AssetForm(Database *db, const std::optional<Asset> &existing, QWidget *parent)
    : QDialog(parent)
    , m_db(db)
    , m_existing(existing)
    , m_assetId(existing ? existing->id : db->nextAssetId())
{
    if (m_existing) m_existingFiles = db->getAssetFiles(m_existing->id);
    setWindowTitle(m_existing ? "Edit Asset — " + m_existing->id : QStringLiteral("Add New Asset"));
    setMinimumWidth(m_existing ? 540 : 520);
    buildUi();
}

void AssetForm::buildUi() {
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);

    auto *idLabel = new QLabel(m_assetId);
    idLabel->setStyleSheet("font-weight: bold; color: #555;");
    form->addRow("Asset ID:", idLabel);

    m_name = new QLineEdit(m_existing ? m_existing->name : QString());
    m_name->setPlaceholderText("e.g. Samsung 65\" TV");
    form->addRow("Name *:", m_name);

    m_category = new QComboBox;
    m_category->addItem("— Select Category —", QString());
    for (const QString &category : Config::categories())
        m_category->addItem(category, category);
    if (m_existing) {
        const int index = m_category->findData(m_existing->category);
        if (index >= 0) m_category->setCurrentIndex(index);
    }
    form->addRow("Category:", m_category);

    m_serial = new QLineEdit(m_existing ? m_existing->serialNumber : QString());
    m_serial->setPlaceholderText("Optional");
    form->addRow("Serial Number:", m_serial);

    m_model = new QLineEdit(m_existing ? m_existing->modelNumber : QString());
    m_model->setPlaceholderText("Optional");
    form->addRow("Model Number:", m_model);

    m_date = new QDateEdit;
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat("yyyy-MM-dd");
    m_noDate = new QCheckBox("Unknown");
    const bool hasDate = m_existing && !m_existing->datePurchase.isEmpty();
    m_date->setDate(hasDate ? QDate::fromString(m_existing->datePurchase, "yyyy-MM-dd")
                            : QDate::currentDate());
    if (m_existing && !hasDate) {
        m_noDate->setChecked(true);
        m_date->setEnabled(false);
    }
    connect(m_noDate, &QCheckBox::toggled, m_date, &QWidget::setDisabled);
    form->addRow("Purchase Date:", rowOf({m_date, m_noDate}));

    m_value = moneyBox();
    m_noValue = new QCheckBox("Unknown");
    if (m_existing) bindValue(m_value, m_noValue, m_existing->valueEstimate);
    else connect(m_noValue, &QCheckBox::toggled, m_value, &QWidget::setDisabled);
    form->addRow("Purchase Price:", rowOf({m_value, m_noValue}));

    m_current = moneyBox();
    m_noCurrent = new QCheckBox("Unknown");
    bindValue(m_current, m_noCurrent, m_existing ? m_existing->currentValue : std::nullopt);
    form->addRow("Current Value:", rowOf({m_current, m_noCurrent}));

    m_hasReceipt = new QCheckBox("I have a sales receipt");
    m_hasReceipt->setChecked(m_existing && m_existing->hasReceipt);
    auto *receiptButton = new QPushButton(m_existing ? "Replace Receipt..." : "Upload Receipt...");
    receiptButton->setEnabled(m_hasReceipt->isChecked());
    m_receiptLabel = new QLabel;
    m_receiptLabel->setStyleSheet("color: gray; font-size: 11px;");
    for (const AssetFile &file : m_existingFiles) {
        if (file.fileType == "receipt") {
            m_receiptLabel->setText("Current: " + file.fileName);
            break;
        }
    }
    connect(m_hasReceipt, &QCheckBox::toggled, receiptButton, &QWidget::setEnabled);
    connect(receiptButton, &QPushButton::clicked, this, &AssetForm::pickReceipt);
    form->addRow("Sales Receipt:", rowOf({m_hasReceipt, receiptButton, m_receiptLabel}, true));

    layout->addLayout(form);

    if (m_existing) {
        layout->addWidget(sectionLabel("Existing Photos  (select + Remove to delete)"));
        m_existingList = new QListWidget;
        m_existingList->setFixedHeight(90);
        m_existingList->setSelectionMode(QAbstractItemView::ExtendedSelection);
        for (int i = 0; i < m_existingFiles.size(); ++i) {
            if (m_existingFiles[i].fileType != "image") continue;
            auto *item = new QListWidgetItem(m_existingFiles[i].fileName);
            item->setData(Qt::UserRole, i);
            m_existingList->addItem(item);
        }
        layout->addWidget(m_existingList);

        auto *removeExisting = new QPushButton("Remove Selected Photos");
        connect(removeExisting, &QPushButton::clicked, this, &AssetForm::removeExistingPhotos);
        layout->addWidget(removeExisting);
    }

    layout->addWidget(sectionLabel(m_existing ? "Add New Photos" : "Photos of Asset"));
    m_newPhotoList = new QListWidget;
    m_newPhotoList->setFixedHeight(m_existing ? 70 : 100);
    m_newPhotoList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(m_newPhotoList);

    auto *photoButtons = new QHBoxLayout;
    auto *addPhotos = new QPushButton("Add Photos...");
    connect(addPhotos, &QPushButton::clicked, this, &AssetForm::pickPhotos);
    auto *removePhotos = new QPushButton("Remove Selected");
    connect(removePhotos, &QPushButton::clicked, this, &AssetForm::removeNewPhotos);
    photoButtons->addWidget(addPhotos);
    photoButtons->addWidget(removePhotos);
    photoButtons->addStretch();
    layout->addLayout(photoButtons);

    auto *note = new QLabel("🔒  Photos and receipts are encrypted on disk using Windows DPAPI.\n"
                            "    Use  File → Export Asset Files  to obtain unencrypted copies.");
    note->setStyleSheet("color: #555; font-size: 10px;");
    note->setWordWrap(true);
    layout->addWidget(note);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &AssetForm::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void AssetForm::pickPhotos() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, "Select Photos", QString(),
        QString("Images (%1);;All Files (*)").arg(Config::filterFor(Config::imageExtensions())));
    for (const QString &path : paths) {
        if (m_newPhotos.contains(path)) continue;
        m_newPhotos << path;
        m_newPhotoList->addItem(QFileInfo(path).fileName());
    }
}

void AssetForm::removeNewPhotos() {
    const QList<QListWidgetItem *> items = m_newPhotoList->selectedItems();
    for (QListWidgetItem *item : items) {
        m_newPhotos.removeAt(m_newPhotoList->row(item));
        delete item;
    }
}

void AssetForm::removeExistingPhotos() {
    const QList<QListWidgetItem *> items = m_existingList->selectedItems();
    for (QListWidgetItem *item : items) {
        m_filesToRemove.append(m_existingFiles[item->data(Qt::UserRole).toInt()]);
        delete item;
    }
}

void AssetForm::pickReceipt() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Select Sales Receipt", QString(),
        QString("Receipt Files (%1);;All Files (*)")
            .arg(Config::filterFor(Config::receiptExtensions())));
    if (path.isEmpty()) return;
    m_newReceipt = path;
    m_receiptLabel->setText((m_existing ? "New: " : "") + QFileInfo(path).fileName());
}

Asset AssetForm::formAsset() const {
    Asset asset;
    asset.id = m_assetId;
    asset.name = m_name->text().trimmed();
    asset.category = m_category->currentData().toString();
    if (!m_noDate->isChecked())
        asset.datePurchase = m_date->date().toString("yyyy-MM-dd");
    if (!m_noValue->isChecked()) asset.valueEstimate = m_value->value();
    if (!m_noCurrent->isChecked()) asset.currentValue = m_current->value();
    asset.serialNumber = m_serial->text().trimmed();
    asset.modelNumber = m_model->text().trimmed();
    return asset;
}

void AssetForm::saveNew(const Asset &formValues) {
    Asset asset = formValues;
    asset.hasReceipt = m_hasReceipt->isChecked() && !m_newReceipt.isEmpty();
    asset.dateAdded = utcNow();
    m_db->insertAsset(asset);

    const auto addFiles = [&](const QStringList &paths, const QString &type) {
        const QVector<AssetFile> files = Storage::importFiles(asset, paths, type);
        for (const AssetFile &file : files)
            m_db->insertAssetFile(file);
    };
    if (!m_newPhotos.isEmpty()) addFiles(m_newPhotos, "image");
    if (asset.hasReceipt) addFiles({m_newReceipt}, "receipt");
}

void AssetForm::saveEdit(const Asset &formValues) {
    Asset updated = formValues;
    updated.hasReceipt = m_hasReceipt->isChecked();
    updated.dateAdded = m_existing->dateAdded;
    updated.notes = m_existing->notes;

    const auto removeFile = [&](const AssetFile &file) {
        QFile::remove(Storage::storedPath(*m_existing, file));
        m_db->deleteAssetFile(file.id);
    };
    for (const AssetFile &file : m_filesToRemove)
        removeFile(file);

    // Replacing a receipt removes the old one before the folder can be renamed.
    if (!m_newReceipt.isEmpty()) {
        for (const AssetFile &file : m_existingFiles) {
            if (file.fileType == "receipt") removeFile(file);
        }
    }

    Storage::renameAssetDir(*m_existing, updated);

    const auto addFiles = [&](const QStringList &paths, const QString &type) {
        const QVector<AssetFile> files = Storage::importFiles(updated, paths, type);
        for (const AssetFile &file : files)
            m_db->insertAssetFile(file);
    };
    if (!m_newPhotos.isEmpty()) addFiles(m_newPhotos, "image");
    if (!m_newReceipt.isEmpty()) addFiles({m_newReceipt}, "receipt");

    m_db->updateAsset(updated);
}

void AssetForm::onAccept() {
    const Asset asset = formAsset();
    if (asset.name.isEmpty()) {
        QMessageBox::warning(this, "Validation", "Asset name is required.");
        return;
    }
    try {
        if (m_existing) saveEdit(asset);
        else saveNew(asset);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Error", QString("Failed to save asset:\n%1").arg(e.what()));
        return;
    }
    accept();
}
