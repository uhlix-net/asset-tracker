#pragma once
#include <QString>
#include <QVector>

#include "models.h"

namespace CsvExport {
QString defaultName();
void write(const QVector<Asset> &assets, const QString &dest);
} // namespace CsvExport
