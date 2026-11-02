#include "previewpanel.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "database.h"
#include "imageviewer.h"
#include "insurerinfodialog.h"
#include "report.h"
#include "storage.h"

namespace {

constexpr int kThumbSize = 160;

class ClickableLabel : public QLabel {
public:
    ClickableLabel(const QString &path, const QString &name, bool encrypted)
        : m_path(path), m_name(name), m_encrypted(encrypted)
    {
        setCursor(Qt::PointingHandCursor);
        setToolTip("Click to view full size: " + name);
    }

protected:
    void mousePressEvent(QMouseEvent *) override {
        ImageViewer(m_path, m_name, m_encrypted, window()).exec();
    }

private:
    QString m_path, m_name;
    bool m_encrypted;
};

QString orDash(const QString &text) { return text.isEmpty() ? QStringLiteral("—") : text; }

} // namespace

PreviewPanel::PreviewPanel(Database *db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
{
    m_notesTimer = new QTimer(this);
    m_notesTimer->setSingleShot(true);
    m_notesTimer->setInterval(500);
    connect(m_notesTimer, &QTimer::timeout, this, &PreviewPanel::saveNotes);
    buildUi();
    clear();
}

void PreviewPanel::buildUi() {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);

    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(m_scroll);

    auto *container = new QWidget;
    m_scroll->setWidget(container);
    auto *layout = new QVBoxLayout(container);
    layout->setAlignment(Qt::AlignTop);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    const auto addRow = [form](const QString &label) {
        auto *value = new QLabel;
        form->addRow(label, value);
        return value;
    };
    m_id = addRow("Asset ID:");
    m_name = addRow("Name:");
    m_name->setWordWrap(true);
    m_category = addRow("Category:");
    m_purchase = addRow("Purchase Date:");
    m_value = addRow("Purchase Price:");
    m_current = addRow("Current Value:");
    m_serial = addRow("Serial Number:");
    m_model = addRow("Model Number:");
    m_receiptLabel = addRow("Receipt:");
    m_receiptLabel->setOpenExternalLinks(false);
    connect(m_receiptLabel, &QLabel::linkActivated, this, &PreviewPanel::openReceipt);
    m_added = addRow("Date Added:");
    layout->addLayout(form);

    auto *photosLabel = new QLabel("Photos  (click to view full size)");
    photosLabel->setStyleSheet("font-weight: bold; margin-top: 8px; color: #1a5276;");
    layout->addWidget(photosLabel);

    auto *photosContainer = new QWidget;
    m_photos = new QGridLayout(photosContainer);
    m_photos->setSpacing(4);
    layout->addWidget(photosContainer);

    auto *notesLabel = new QLabel("Notes");
    notesLabel->setStyleSheet("font-weight: bold; margin-top: 8px;");
    layout->addWidget(notesLabel);
    m_notes = new QTextEdit;
    m_notes->setFixedHeight(80);
    m_notes->setAcceptRichText(false);
    connect(m_notes, &QTextEdit::textChanged, m_notesTimer, qOverload<>(&QTimer::start));
    layout->addWidget(m_notes);

    m_open = new QPushButton("Open Asset Folder");
    connect(m_open, &QPushButton::clicked, this, &PreviewPanel::openFolder);
    layout->addWidget(m_open);

    m_print = new QPushButton("Print This Asset");
    connect(m_print, &QPushButton::clicked, this, &PreviewPanel::printAsset);
    layout->addWidget(m_print);
    layout->addStretch();
}

void PreviewPanel::showAsset(const Asset &asset) {
    // A pending notes edit belongs to the asset being left.
    if (m_notesTimer->isActive()) {
        m_notesTimer->stop();
        saveNotes();
    }

    m_asset = asset;
    const QVector<AssetFile> files = m_db->getAssetFiles(asset.id);

    m_id->setText(asset.id);
    m_name->setText(asset.name);
    m_category->setText(orDash(asset.category));
    m_purchase->setText(orDash(asset.datePurchase));
    m_value->setText(moneyDisplay(asset.valueEstimate));
    m_current->setText(moneyDisplay(asset.currentValue));
    m_serial->setText(orDash(asset.serialNumber));
    m_model->setText(orDash(asset.modelNumber));
    m_added->setText(asset.dateAdded.left(10));

    m_receipt.reset();
    for (const AssetFile &file : files) {
        if (file.fileType == "receipt") {
            m_receipt = file;
            break;
        }
    }
    if (asset.hasReceipt && m_receipt) {
        m_receiptLabel->setText("<a href=\"open\" style=\"color: #27ae60; text-decoration: none;\">"
                                "Receipt on file <span style=\"font-weight: bold;\">✓</span></a>");
    } else {
        m_receipt.reset();
        m_receiptLabel->setText("No receipt");
    }

    m_notes->blockSignals(true);
    m_notes->setPlainText(asset.notes);
    m_notes->blockSignals(false);
    m_notes->setEnabled(true);
    m_open->setEnabled(true);
    m_print->setEnabled(true);

    loadPhotos(asset, files);
}

// The scroll area does not hear about the photo grid growing, so without this it
// keeps its old size: the rows squeeze on top of each other and no scrollbar shows.
// Deferred so the nested layouts have reported their new size first.
void PreviewPanel::updateScrollArea() {
    QTimer::singleShot(0, m_scroll, [this] {
        QEvent request(QEvent::LayoutRequest);
        QCoreApplication::sendEvent(m_scroll, &request);
    });
}

void PreviewPanel::clearPhotos() {
    while (QLayoutItem *item = m_photos->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void PreviewPanel::loadPhotos(const Asset &asset, const QVector<AssetFile> &files) {
    clearPhotos();

    int index = 0;
    for (const AssetFile &file : files) {
        if (file.fileType != "image") continue;
        const QString path = Storage::storedPath(asset, file);
        auto *label = new ClickableLabel(path, file.fileName, file.encrypted);
        label->setFixedSize(kThumbSize + 4, kThumbSize + 4);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("border: 1px solid #aaa;");

        const QPixmap pixmap = Storage::thumbnail(path, kThumbSize, file.encrypted);
        if (pixmap.isNull()) {
            label->setText(file.fileName);
            label->setWordWrap(true);
        } else {
            label->setPixmap(pixmap);
        }
        m_photos->addWidget(label, index / 2, index % 2);
        ++index;
    }

    if (index == 0) {
        auto *none = new QLabel("No photos");
        none->setStyleSheet("color: gray;");
        m_photos->addWidget(none, 0, 0);
    }
    updateScrollArea();
}

void PreviewPanel::clear() {
    if (m_notesTimer->isActive()) {
        m_notesTimer->stop();
        saveNotes();
    }
    m_asset.reset();
    m_receipt.reset();
    for (QLabel *label : {m_id, m_name, m_category, m_purchase, m_value, m_current,
                          m_serial, m_model, m_receiptLabel, m_added})
        label->clear();
    m_notes->blockSignals(true);
    m_notes->clear();
    m_notes->blockSignals(false);
    m_notes->setEnabled(false);
    m_open->setEnabled(false);
    m_print->setEnabled(false);
    clearPhotos();
    updateScrollArea();
}

void PreviewPanel::saveNotes() {
    if (!m_asset) return;
    try {
        m_db->updateNotes(m_asset->id, m_notes->toPlainText());
        m_asset->notes = m_notes->toPlainText();
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Error", QString("Could not save notes:\n%1").arg(e.what()));
    }
}

void PreviewPanel::printAsset() {
    if (!m_asset) return;
    const QString suggested = QString("Asset_%1_%2.pdf").arg(
        m_asset->id, m_asset->name.left(30).replace(' ', '_'));
    const QString dest = QFileDialog::getSaveFileName(
        this, "Save Asset Record — " + m_asset->name, suggested, "PDF Files (*.pdf)");
    if (dest.isEmpty()) return;

    try {
        Report::generateSingle(*m_asset, m_db->getAssetFiles(m_asset->id), dest,
                               loadInsurerInfo());
        if (QMessageBox::information(this, "Asset Record Saved",
                                     QString("Saved to:\n%1\n\nOpen file?").arg(dest),
                                     QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
            QDesktopServices::openUrl(QUrl::fromLocalFile(dest));
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Print Failed", e.what());
    }
}

// The stored file is DPAPI-encrypted, so handing it straight to the default
// viewer shows garbage. Give the viewer a decrypted copy instead.
void PreviewPanel::openReceipt() {
    if (!m_asset || !m_receipt) return;
    try {
        if (!m_tempDir) m_tempDir = std::make_unique<QTemporaryDir>();
        const QString copy = m_tempDir->filePath(m_receipt->fileName);
        Storage::writeFile(copy, Storage::readFile(Storage::storedPath(*m_asset, *m_receipt),
                                                   m_receipt->encrypted));
        QDesktopServices::openUrl(QUrl::fromLocalFile(copy));
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Receipt", e.what());
    }
}

void PreviewPanel::openFolder() {
    if (!m_asset) return;
    const QString dir = Storage::assetDir(*m_asset);
    if (QFileInfo::exists(dir))
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
