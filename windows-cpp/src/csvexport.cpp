#include "csvexport.h"

#include <QDate>
#include <QRegularExpression>
#include <QStringList>

#include "storage.h"

namespace {

QString field(const QString &value) {
    if (!value.contains(QRegularExpression(R"([",\r\n])"))) return value;
    return '"' + QString(value).replace('"', "\"\"") + '"';
}

QString plainMoney(std::optional<double> value) {
    return value ? QString::number(*value, 'f', 2) : QString();
}

} // namespace

QString CsvExport::defaultName() {
    return QString("asset_inventory_%1.csv").arg(QDate::currentDate().toString("yyyy-MM-dd"));
}

void CsvExport::write(const QVector<Asset> &assets, const QString &dest) {
    QStringList lines = {
        "Asset ID,Name,Category,Serial Number,Model Number,Purchase Date,"
        "Purchase Price,Current Value,Receipt on File,Date Added,Notes"
    };
    for (const Asset &a : assets) {
        const QStringList row = {
            a.id, a.name, a.category, a.serialNumber, a.modelNumber, a.datePurchase,
            plainMoney(a.valueEstimate), plainMoney(a.currentValue),
            a.hasReceipt ? "Yes" : "No", a.dateAdded.left(10), a.notes,
        };
        QStringList cells;
        for (const QString &cell : row)
            cells << field(cell);
        lines << cells.join(',');
    }
    // BOM so Excel reads it as UTF-8.
    Storage::writeFile(dest, "\xEF\xBB\xBF" + (lines.join("\r\n") + "\r\n").toUtf8());
}
