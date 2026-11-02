#include "models.h"

#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>

QString slugify(const QString &text) {
    static const QRegularExpression strip(
        R"([^\w\s-])", QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression runs(
        R"([\s_-]+)", QRegularExpression::UseUnicodePropertiesOption);

    QString slug = text.toLower().trimmed();
    slug.remove(strip);
    slug.replace(runs, "_");

    // Python's [:40] counts code points, not UTF-16 units.
    const QList<uint> points = slug.toUcs4();
    return points.size() <= 40 ? slug : QString::fromUcs4(points.constData(), 40);
}

QString moneyDisplay(std::optional<double> value) {
    if (!value) return QStringLiteral("—");
    static const QLocale us(QLocale::English, QLocale::UnitedStates);
    return "$" + us.toString(*value, 'f', 2);
}

QString utcNow() {
    return QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ssZ");
}
