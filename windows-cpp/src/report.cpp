#include "report.h"

#include <QBuffer>
#include <QDate>
#include <QEventLoop>
#include <QLocale>
#include <QFileInfo>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QTextLayout>
#include <algorithm>
#include <memory>
#include <stdexcept>

#include "config.h"
#include "storage.h"

namespace {

// Device units per point. Layout is done in points; the painter gets device units.
constexpr int kDpi = 1200;
constexpr qreal kScale = kDpi / 72.0;
constexpr qreal kInch = 72.0;

constexpr qreal kPageW = 8.5 * kInch;
constexpr qreal kPageH = 11.0 * kInch;
constexpr qreal kMargin = 1.0 * kInch;
constexpr qreal kBottom = kPageH - 0.85 * kInch;
constexpr qreal kContentW = kPageW - 2 * kMargin;

// Images are scaled to this before embedding; phone photos would otherwise make
// the PDF enormous without looking any better at print size.
constexpr int kEmbedDpi = 300;
constexpr qreal kPdfReceiptZoom = 2.0;

const QColor kNavy("#1B2A3B");
const QColor kNavyMid("#2C3E50");
const QColor kSlate("#4A5568");
const QColor kRule("#8896A6");
const QColor kLightBg("#F4F6F8");

const QString kDash = QStringLiteral("—");

struct Style {
    qreal size;
    qreal leading;
    QColor color;
    bool bold = false;
    Qt::Alignment align = Qt::AlignLeft;
    qreal tracking = 0;
};

const Style kDocTitle   {22, 28, kNavy, true, Qt::AlignHCenter};
const Style kDocSubtitle{11, 15, kSlate, false, Qt::AlignHCenter};
const Style kSectionHead{9, 12, kSlate, true, Qt::AlignLeft, 1.2};
const Style kDeclaration{9, 14, Qt::black};
const Style kCatHeading {13, 17, Qt::white, true};
const Style kCatCount   {9, 12, Qt::white, false, Qt::AlignRight};
const Style kAssetId    {10, 14, kSlate, true, Qt::AlignRight};
const Style kAssetName  {13, 17, kNavy, true};
const Style kFieldLabel {8, 11, kSlate, true};
const Style kFieldValue {9, 12, Qt::black};
const Style kFieldRight {9, 12, Qt::black, false, Qt::AlignRight};
const Style kCaption    {7, 9, kSlate, false, Qt::AlignHCenter};
const Style kFootNote   {7, 10, kSlate, false, Qt::AlignHCenter};
const Style kTocHead    {14, 18, kNavy, true};
const Style kHeader     {7.5, 10, kSlate};
const Style kSummaryKey {10, 13, kSlate, true};
const Style kSummaryVal {10, 13, Qt::black};
const Style kSigLabel   {8, 11, kSlate};

struct Cell {
    QString text;
    Style style;
};

class Pdf {
public:
    Pdf(const QString &dest, const QString &date)
        : m_writer(dest), m_date(date)
    {
        m_writer.setResolution(kDpi);
        m_writer.setPageLayout(QPageLayout(QPageSize(QPageSize::Letter), QPageLayout::Portrait,
                                           QMarginsF(0, 0, 0, 0)));
        m_writer.setTitle("Personal Property Inventory Report");
        m_writer.setCreator(Config::kAppName);
        if (!m_painter.begin(&m_writer))
            throw std::runtime_error("Could not write the PDF. Is the file open elsewhere?");
    }

    qreal y() const { return m_y; }

    void finish() {
        drawPageFurniture();
        if (!m_painter.end())
            throw std::runtime_error("Could not finish writing the PDF.");
    }

    void newPage() {
        drawPageFurniture();
        m_writer.newPage();
        ++m_page;
        m_y = kMargin;
    }

    void space(qreal height) { m_y += height; }

    void ensure(qreal height) {
        if (m_y + height > kBottom && m_y > kMargin) newPage();
    }

    void rule(qreal thickness, const QColor &color, qreal before = 4, qreal after = 4) {
        m_y += before;
        line(kMargin, m_y, kMargin + kContentW, m_y, thickness, color);
        m_y += after;
    }

    qreal textHeight(const QString &text, const Style &style, qreal width) {
        return paragraph(text, style, width, nullptr, 0, 0);
    }

    void text(const QString &text, const Style &style, qreal x, qreal y, qreal width) {
        paragraph(text, style, width, &m_painter, x, y);
    }

    // A full-width paragraph that continues onto a new page when it has to.
    void flow(const QString &text, const Style &style, qreal x = kMargin, qreal width = kContentW) {
        for (const QString &line : wrap(text, style, width)) {
            ensure(style.leading);
            this->text(line, style, x, m_y, width);
            m_y += style.leading;
        }
    }

    // A table row of wrapped cells; returns its height. Moves to a new page first
    // if the row will not fit.
    qreal row(qreal x, const QVector<qreal> &widths, const QVector<Cell> &cells, qreal padding,
              const QColor &background, qreal gridWidth) {
        const qreal height = rowHeight(widths, cells, padding);
        ensure(height);
        drawRow(x, m_y, widths, cells, height, padding, background, gridWidth);
        m_y += height;
        return height;
    }

    qreal rowHeight(const QVector<qreal> &widths, const QVector<Cell> &cells, qreal padding) {
        qreal height = 0;
        for (int i = 0; i < cells.size(); ++i) {
            height = qMax(height, textHeight(cells[i].text, cells[i].style,
                                             widths[i] - 2 * padding));
        }
        return height + 2 * padding;
    }

    void drawRow(qreal x, qreal y, const QVector<qreal> &widths, const QVector<Cell> &cells,
                 qreal height, qreal padding, const QColor &background, qreal gridWidth) {
        qreal cellX = x;
        for (int i = 0; i < cells.size(); ++i) {
            const QRectF cell(cellX, y, widths[i], height);
            if (background.isValid()) fill(cell, background);
            if (gridWidth > 0) box(cell, gridWidth, kRule);
            text(cells[i].text, cells[i].style, cellX + padding, y + padding,
                 widths[i] - 2 * padding);
            cellX += widths[i];
        }
    }

    void fill(const QRectF &rect, const QColor &color) {
        m_painter.fillRect(device(rect), color);
    }

    void box(const QRectF &rect, qreal thickness, const QColor &color) {
        m_painter.setPen(QPen(color, thickness * kScale));
        m_painter.setBrush(Qt::NoBrush);
        m_painter.drawRect(device(rect));
    }

    void line(qreal x1, qreal y1, qreal x2, qreal y2, qreal thickness, const QColor &color) {
        m_painter.setPen(QPen(color, thickness * kScale));
        m_painter.drawLine(QPointF(x1, y1) * kScale, QPointF(x2, y2) * kScale);
    }

    void image(const QImage &image, const QRectF &rect) {
        m_painter.drawImage(device(rect), image);
    }

private:
    static QRectF device(const QRectF &rect) {
        return QRectF(rect.topLeft() * kScale, rect.size() * kScale);
    }

    QFont fontFor(const Style &style) const {
        QFont font("Arial");
        font.setPointSizeF(style.size);
        font.setBold(style.bold);
        font.setHintingPreference(QFont::PreferNoHinting);
        // In 96-dpi pixels: Qt scales this to the paint device itself.
        if (style.tracking > 0)
            font.setLetterSpacing(QFont::AbsoluteSpacing, style.tracking * 96.0 / 72.0);
        return font;
    }

    // Lays text out at the device resolution. Returns the height in points, and
    // draws when given a painter.
    qreal paragraph(const QString &text, const Style &style, qreal width, QPainter *painter,
                    qreal x, qreal y) {
        const qreal deviceWidth = width * kScale;
        const qreal leading = style.leading * kScale;
        qreal top = 0;

        const QStringList blocks = text.split('\n');
        for (const QString &block : blocks) {
            QTextLayout layout(block, fontFor(style), &m_writer);
            QTextOption option;
            option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            option.setAlignment(style.align);
            layout.setTextOption(option);

            layout.beginLayout();
            for (;;) {
                QTextLine line = layout.createLine();
                if (!line.isValid()) break;
                line.setLineWidth(deviceWidth);
                line.setPosition(QPointF(0, top + (leading - line.height()) / 2));
                top += leading;
            }
            layout.endLayout();

            if (painter) {
                painter->setPen(style.color);
                layout.draw(painter, QPointF(x * kScale, y * kScale));
            }
        }
        return top / kScale;
    }

    // The source text broken into the lines paragraph() would produce.
    QStringList wrap(const QString &text, const Style &style, qreal width) {
        QStringList lines;
        const QStringList blocks = text.split('\n');
        for (const QString &block : blocks) {
            QTextLayout layout(block, fontFor(style), &m_writer);
            QTextOption option;
            option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            layout.setTextOption(option);
            layout.beginLayout();
            for (;;) {
                QTextLine line = layout.createLine();
                if (!line.isValid()) break;
                line.setLineWidth(width * kScale);
                lines << block.mid(line.textStart(), line.textLength()).trimmed();
            }
            layout.endLayout();
            if (block.isEmpty()) lines << QString();
        }
        return lines;
    }

    void drawPageFurniture() {
        Style left = kHeader;
        Style centre = kHeader;
        centre.align = Qt::AlignHCenter;
        Style right = kHeader;
        right.align = Qt::AlignRight;

        if (m_page > 1) {
            const qreal top = kMargin * 0.45;
            text("PERSONAL PROPERTY INVENTORY REPORT", left, kMargin, top - 8, kContentW);
            text("Prepared: " + m_date, right, kMargin, top - 8, kContentW);
            line(kMargin, kMargin * 0.6, kMargin + kContentW, kMargin * 0.6, 0.5, kRule);
        }

        const qreal rule = kPageH - 0.55 * kInch;
        const qreal base = kPageH - 0.46 * kInch;
        line(kMargin, rule, kMargin + kContentW, rule, 0.5, kRule);
        text("CONFIDENTIAL — For Insurance Claim Purposes Only", left, kMargin, base, kContentW);
        text(QString("Page %1").arg(m_page), centre, kMargin, base, kContentW);
        text(m_date, right, kMargin, base, kContentW);
    }

    QPdfWriter m_writer;
    QPainter m_painter;
    QString m_date;
    int m_page = 1;
    qreal m_y = kMargin;
};

QString today() {
    return QLocale(QLocale::English, QLocale::UnitedStates)
        .toString(QDate::currentDate(), "MMMM dd, yyyy");
}

QString orDash(const QString &text) { return text.isEmpty() ? kDash : text; }

bool isImage(const QString &path) {
    return Config::imageExtensions().contains("." + QFileInfo(path).suffix().toLower());
}

// The pages of a PDF held in memory, so a decrypted receipt never reaches disk.
QVector<QImage> renderPdfPages(QByteArray data) {
    QVector<QImage> pages;
    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);

    QPdfDocument document;
    QEventLoop loop;
    QObject::connect(&document, &QPdfDocument::statusChanged, &loop, [&](QPdfDocument::Status s) {
        if (s == QPdfDocument::Status::Ready || s == QPdfDocument::Status::Error) loop.quit();
    });
    document.load(&buffer);
    if (document.status() == QPdfDocument::Status::Loading) loop.exec();
    if (document.status() != QPdfDocument::Status::Ready) return pages;

    for (int i = 0; i < document.pageCount(); ++i) {
        const QSizeF points = document.pagePointSize(i);
        const QImage page = document.render(i, (points * kPdfReceiptZoom).toSize());
        if (!page.isNull()) pages.append(page);
    }
    return pages;
}

QSizeF fitted(const QSize &size, qreal maxW, qreal maxH) {
    return QSizeF(size).scaled(maxW, maxH, Qt::KeepAspectRatio);
}

void claimTable(Pdf &pdf, const InsurerInfo &info, qreal size, qreal padding) {
    if (info.isEmpty()) return;
    Style key = kSummaryKey;
    Style value = kSummaryVal;
    key.size = value.size = size;

    const qreal widths[] = {2.0 * kInch, 4.1 * kInch};
    const qreal x = kMargin + (kContentW - widths[0] - widths[1]) / 2;
    const struct { const char *label; const QString &value; } rows[] = {
        {"Insurance Company:", info.company},
        {"Policy Number:",     info.policyNumber},
        {"Claim Number:",      info.claimNumber},
        {"Date of Loss:",      info.dateOfLoss},
    };
    for (const auto &row : rows) {
        if (row.value.isEmpty()) continue;
        pdf.row(x, {widths[0], widths[1]}, {{row.label, key}, {row.value, value}},
                padding, kLightBg, 0.4);
    }
}

void titlePage(Pdf &pdf, const QVector<AssetRecord> &records, const InsurerInfo &insurer,
               const QString &date) {
    std::optional<double> purchaseTotal, currentTotal;
    for (const AssetRecord &record : records) {
        if (record.asset.valueEstimate)
            purchaseTotal = purchaseTotal.value_or(0) + *record.asset.valueEstimate;
        if (record.asset.currentValue)
            currentTotal = currentTotal.value_or(0) + *record.asset.currentValue;
    }

    pdf.space(1.2 * kInch);
    pdf.rule(1.5, kNavy, 0, 10);
    pdf.flow("PERSONAL PROPERTY INVENTORY REPORT", kDocTitle);
    pdf.space(4);
    pdf.flow("Prepared for Insurance Claim Purposes", kDocSubtitle);
    pdf.rule(1.5, kNavy, 12, 20);

    claimTable(pdf, insurer, 10, 7);
    if (!insurer.isEmpty()) pdf.space(0.15 * kInch);

    const auto total = [](std::optional<double> value) {
        return value ? moneyDisplay(value) : QStringLiteral("Not recorded");
    };
    const QVector<QPair<QString, QString>> summary = {
        {"Date of Preparation:",     date},
        {"Total Items Inventoried:", QString::number(records.size())},
        {"Total Purchase Price:",    total(purchaseTotal)},
        {"Total Current Value:",     total(currentTotal)},
    };
    const QVector<qreal> widths = {2.6 * kInch, 3.5 * kInch};
    const qreal x = kMargin + (kContentW - widths[0] - widths[1]) / 2;
    bool shaded = true;
    for (const auto &row : summary) {
        pdf.row(x, widths, {{row.first, kSummaryKey}, {row.second, kSummaryVal}}, 7,
                shaded ? kLightBg : QColor(Qt::white), 0.4);
        shaded = !shaded;
    }
    pdf.space(0.3 * kInch);

    pdf.rule(0.5, kNavyMid);
    pdf.space(0.1 * kInch);
    pdf.flow("DECLARATION", kSectionHead);
    pdf.space(0.05 * kInch);
    pdf.flow("I hereby certify that this inventory represents a true and accurate accounting "
             "of the personal property described herein, prepared in good faith for the purposes "
             "of filing an insurance claim. All monetary values listed are estimates or documented "
             "purchase prices unless otherwise noted. Photographic evidence and supporting "
             "receipts, where available, are included in this report.", kDeclaration);
    pdf.space(0.25 * kInch);

    const auto signature = [&pdf](const QString &label) {
        const qreal left = kMargin + (kContentW - 5.0 * kInch) / 2;
        pdf.text(label, kSigLabel, left, pdf.y(), 1.5 * kInch);
        pdf.space(kSigLabel.leading + 2);
        pdf.line(left + 1.5 * kInch, pdf.y(), left + 5.0 * kInch, pdf.y(), 0.75, Qt::black);
    };
    signature("Owner / Claimant:");
    pdf.space(0.2 * kInch);
    signature("Date:");
    pdf.space(0.35 * kInch);

    pdf.rule(0.5, kRule);
    pdf.space(0.05 * kInch);
    pdf.flow(QString("This document was generated by %1 software on %2. The accuracy of the "
                     "information contained herein is the sole responsibility of the preparer. "
                     "This report does not constitute a legal appraisal.")
                 .arg(Config::kAppName, date), kFootNote);
    pdf.newPage();
}

void tocPage(Pdf &pdf, const QVector<AssetRecord> &records) {
    pdf.flow("TABLE OF CONTENTS", kTocHead);
    pdf.space(8);
    pdf.rule(0.5, kNavy);
    pdf.space(0.18 * kInch);

    const QVector<qreal> widths = {0.55 * kInch, 1.7 * kInch, 0.95 * kInch, 0.8 * kInch,
                                   1.0 * kInch, 1.0 * kInch, 0.5 * kInch};
    Style head = kFieldLabel;
    head.color = kNavy;
    Style headRight = head;
    headRight.align = Qt::AlignRight;
    const QVector<Cell> header = {
        {"Asset ID", head}, {"Description", head}, {"Category", head}, {"Purchase Date", head},
        {"Purchase Price", headRight}, {"Current Value", headRight}, {"Photos", head},
    };
    const auto drawHeader = [&] {
        pdf.row(kMargin, widths, header, 4, Qt::white, 0.3);
        pdf.line(kMargin, pdf.y(), kMargin + kContentW, pdf.y(), 1.2, kNavy);
    };
    drawHeader();

    bool shaded = false;
    for (const AssetRecord &record : records) {
        const Asset &a = record.asset;
        const auto images = std::count_if(record.files.cbegin(), record.files.cend(),
            [](const AssetFile &f) { return f.fileType == "image"; });
        const QVector<Cell> cells = {
            {a.id, kFieldValue}, {a.name.left(42), kFieldValue},
            {orDash(a.category), kFieldValue}, {orDash(a.datePurchase), kFieldValue},
            {moneyDisplay(a.valueEstimate), kFieldRight},
            {moneyDisplay(a.currentValue), kFieldRight},
            {QString::number(images), kFieldValue},
        };
        const qreal height = pdf.rowHeight(widths, cells, 4);
        if (pdf.y() + height > kBottom) {
            pdf.newPage();
            drawHeader();
        }
        pdf.row(kMargin, widths, cells, 4, shaded ? kLightBg : QColor(Qt::white), 0.3);
        shaded = !shaded;
    }
    pdf.newPage();
}

void categoryDivider(Pdf &pdf, const QString &category, int count) {
    const QString label = category.isEmpty() ? QStringLiteral("Uncategorized") : category;
    const QString items = QString("%1 item%2").arg(count).arg(count == 1 ? "" : "s");
    const QVector<qreal> widths = {kContentW * 0.75, kContentW * 0.25};
    const QVector<Cell> cells = {{label.toUpper(), kCatHeading}, {items, kCatCount}};
    const qreal height = pdf.rowHeight(widths, cells, 8);
    pdf.ensure(height);
    pdf.fill(QRectF(kMargin, pdf.y(), kContentW, height), kNavy);  // One bar: no seam between cells.
    pdf.row(kMargin, widths, cells, 8, QColor(), 0);
    pdf.space(0.2 * kInch);
}

void photoSection(Pdf &pdf, const Asset &asset, const QVector<AssetFile> &images) {
    pdf.space(0.1 * kInch);
    pdf.ensure(kSectionHead.leading + 2.3 * kInch);
    pdf.flow("PHOTOGRAPHIC DOCUMENTATION", kSectionHead);
    pdf.space(0.04 * kInch);

    constexpr int kColumns = 3;
    const qreal cellW = kContentW / kColumns;
    const qreal imageMax = cellW - 8;
    const int embedPixels = qRound(imageMax / kInch * kEmbedDpi);

    for (int start = 0; start < images.size(); start += kColumns) {
        const int end = qMin(start + kColumns, images.size());

        struct Photo { QImage image; QSizeF size; qreal captionH; };
        QVector<Photo> photos;
        qreal rowH = 0;
        for (int i = start; i < end; ++i) {
            Photo photo;
            try {
                photo.image = Storage::loadImage(Storage::storedPath(asset, images[i]),
                                                 images[i].encrypted, embedPixels);
            } catch (const std::exception &) {}
            photo.size = photo.image.isNull() ? QSizeF()
                                              : fitted(photo.image.size(), imageMax, imageMax);
            photo.captionH = pdf.textHeight(images[i].fileName, kCaption, imageMax);
            rowH = qMax(rowH, photo.size.height() + 2 + photo.captionH);
            photos.append(photo);
        }

        pdf.ensure(rowH + 8);
        for (int i = 0; i < photos.size(); ++i) {
            const Photo &photo = photos[i];
            const qreal x = kMargin + i * cellW + 4;
            qreal y = pdf.y() + 4;
            if (photo.image.isNull()) {
                pdf.text("[" + images[start + i].fileName + "]", kFieldLabel, x, y, imageMax);
                continue;
            }
            pdf.image(photo.image, QRectF(x + (imageMax - photo.size.width()) / 2, y,
                                          photo.size.width(), photo.size.height()));
            y += photo.size.height() + 2;
            pdf.text(images[start + i].fileName, kCaption, x, y, imageMax);
        }
        pdf.space(rowH + 8);
    }
}

void receiptSection(Pdf &pdf, const Asset &asset, const AssetFile &receipt) {
    const QString path = Storage::storedPath(asset, receipt);
    const int embedPixels = qRound(kContentW * 1.3 / kInch * kEmbedDpi);

    // Loaded first, so the heading can go on the page the first image lands on.
    QVector<QImage> pages;
    try {
        if (isImage(path)) {
            pages.append(Storage::loadImage(path, receipt.encrypted, embedPixels));
        } else if (QFileInfo(path).suffix().compare("pdf", Qt::CaseInsensitive) == 0) {
            pages = renderPdfPages(Storage::readFile(path, receipt.encrypted));
        }
    } catch (const std::exception &) {}
    pages.removeIf([](const QImage &page) { return page.isNull(); });

    const auto sizeOf = [](const QImage &image) {
        return fitted(image.size(), kContentW, kContentW * 1.3);
    };
    const qreal headingH = 0.1 * kInch + kSectionHead.leading + 0.06 * kInch;
    const qreal firstH = pages.isEmpty() ? kFieldValue.leading
                                         : sizeOf(pages.first()).height() + kCaption.leading + 2;
    pdf.ensure(headingH + firstH);
    pdf.space(0.1 * kInch);
    pdf.flow("SALES RECEIPT", kSectionHead);
    pdf.space(0.06 * kInch);

    if (pages.isEmpty()) {
        pdf.flow(QString("Receipt on file: %1 (could not be included)").arg(receipt.fileName),
                 kFieldValue);
        return;
    }
    for (int i = 0; i < pages.size(); ++i) {
        const QSizeF size = sizeOf(pages[i]);
        pdf.ensure(size.height() + kCaption.leading + 2);
        pdf.image(pages[i], QRectF(kMargin + (kContentW - size.width()) / 2, pdf.y(),
                                   size.width(), size.height()));
        pdf.space(size.height() + 2);
        pdf.flow(pages.size() == 1 ? receipt.fileName
                     : QString("Page %1 of %2 — %3").arg(i + 1).arg(pages.size())
                           .arg(receipt.fileName), kCaption);
    }
}

void assetRecord(Pdf &pdf, const Asset &asset, const QVector<AssetFile> &files) {
    const QVector<qreal> headerW = {kContentW * 0.72, kContentW * 0.28};
    const QVector<Cell> header = {{asset.name, kAssetName},
                                  {"Asset ID: " + asset.id, kAssetId}};

    const QVector<QPair<QString, QString>> left = {
        {"CATEGORY",      orDash(asset.category)},
        {"SERIAL NUMBER", orDash(asset.serialNumber)},
        {"MODEL NUMBER",  orDash(asset.modelNumber)},
        {"DATE ADDED",    orDash(asset.dateAdded.left(10))},
    };
    const QVector<QPair<QString, QString>> right = {
        {"PURCHASE DATE",  orDash(asset.datePurchase)},
        {"PURCHASE PRICE", moneyDisplay(asset.valueEstimate)},
        {"CURRENT VALUE",  moneyDisplay(asset.currentValue)},
    };
    const QVector<qreal> miniW = {1.15 * kInch, 1.85 * kInch};

    const auto miniHeight = [&](const QVector<QPair<QString, QString>> &rows) {
        qreal height = 0;
        for (const auto &row : rows)
            height += pdf.rowHeight(miniW, {{row.first, kFieldLabel}, {row.second, kFieldValue}}, 3);
        return height;
    };
    const qreal detailH = qMax(miniHeight(left), miniHeight(right));

    // Name and details stay on one page; photos and receipts may follow on the next.
    pdf.ensure(pdf.rowHeight(headerW, header, 2) + 6 + 0.08 * kInch + detailH);
    pdf.row(kMargin, headerW, header, 2, QColor(), 0);
    pdf.space(4);
    pdf.line(kMargin, pdf.y() - 2, kMargin + kContentW, pdf.y() - 2, 0.75, kNavy);
    pdf.space(0.08 * kInch);

    const auto miniTable = [&](const QVector<QPair<QString, QString>> &rows, qreal x) {
        qreal y = pdf.y();
        bool shaded = true;
        for (const auto &row : rows) {
            const QVector<Cell> cells = {{row.first, kFieldLabel}, {row.second, kFieldValue}};
            const qreal height = pdf.rowHeight(miniW, cells, 3);
            pdf.drawRow(x, y, miniW, cells, height, 3, shaded ? kLightBg : QColor(Qt::white), 0.3);
            y += height;
            shaded = !shaded;
        }
    };
    miniTable(left, kMargin + 0.1 * kInch);
    miniTable(right, kMargin + 0.1 * kInch + 3.15 * kInch + 6);
    pdf.space(detailH);

    if (!asset.notes.trimmed().isEmpty()) {
        pdf.space(0.06 * kInch);
        const qreal labelW = 0.9 * kInch;
        const qreal valueW = kContentW - labelW;
        const qreal height = pdf.textHeight(asset.notes, kFieldValue, valueW - 8) + 8;
        if (height < kBottom - kMargin) {
            pdf.row(kMargin, {labelW, valueW}, {{"NOTES", kFieldLabel}, {asset.notes, kFieldValue}},
                    4, kLightBg, 0.3);
        } else {
            // Too long for any one page, so it runs on without the table cell.
            pdf.flow("NOTES", kFieldLabel);
            pdf.flow(asset.notes, kFieldValue);
        }
    }

    QVector<AssetFile> images;
    const AssetFile *receipt = nullptr;
    for (const AssetFile &file : files) {
        if (file.fileType == "image") images.append(file);
        else if (file.fileType == "receipt" && !receipt) receipt = &file;
    }
    if (!images.isEmpty()) photoSection(pdf, asset, images);
    if (receipt) receiptSection(pdf, asset, *receipt);

    pdf.space(0.14 * kInch);
    pdf.rule(0.75, kRule);
    pdf.space(0.1 * kInch);
}

} // namespace

void Report::generate(const QVector<AssetRecord> &records, const QString &dest,
                      const InsurerInfo &insurer) {
    const QString date = today();
    Pdf pdf(dest, date);
    titlePage(pdf, records, insurer, date);
    tocPage(pdf, records);

    // Blank categories sort last; a page break separates categories.
    const auto sortKey = [](const AssetRecord &r) {
        return r.asset.category.isEmpty() ? QStringLiteral("zzz") : r.asset.category;
    };
    QVector<AssetRecord> sorted = records;
    std::stable_sort(sorted.begin(), sorted.end(),
        [&](const AssetRecord &a, const AssetRecord &b) {
            const QString ka = sortKey(a), kb = sortKey(b);
            return ka != kb ? ka < kb : a.asset.id < b.asset.id;
        });

    for (int i = 0; i < sorted.size();) {
        int end = i;
        while (end < sorted.size() && sorted[end].asset.category == sorted[i].asset.category)
            ++end;
        if (i > 0) pdf.newPage();
        categoryDivider(pdf, sorted[i].asset.category, end - i);
        for (; i < end; ++i)
            assetRecord(pdf, sorted[i].asset, sorted[i].files);
    }
    pdf.finish();
}

void Report::generateSingle(const Asset &asset, const QVector<AssetFile> &files,
                            const QString &dest, const InsurerInfo &insurer) {
    Pdf pdf(dest, today());
    pdf.rule(1.5, kNavy, 0, 8);
    pdf.flow("ASSET RECORD", kDocTitle);
    pdf.space(4);
    pdf.flow("Personal Property Inventory — Insurance Claim Purposes", kDocSubtitle);
    pdf.rule(1.5, kNavy, 10, 12);

    claimTable(pdf, insurer, 9, 5);
    if (!insurer.isEmpty()) pdf.space(0.1 * kInch);

    assetRecord(pdf, asset, files);
    pdf.finish();
}
