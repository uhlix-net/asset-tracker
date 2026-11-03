#pragma once
#include <QColor>
#include <QIcon>
#include <QString>

// Line icons drawn in code, so there are no image assets to keep in step.
//
// Each icon is rendered at 1x, 2x and 3x so Windows display scaling picks a
// sharp one, and cached per (icon, size, colour).
namespace Icons {

QIcon add(const QColor &color, int size = 16);
QIcon edit(const QColor &color, int size = 16);
QIcon duplicate(const QColor &color, int size = 16);
QIcon remove(const QColor &color, int size = 16);
QIcon backup(const QColor &color, int size = 16);
QIcon print(const QColor &color, int size = 16);
QIcon search(const QColor &color, int size = 16);
QIcon folder(const QColor &color, int size = 16);
QIcon photo(const QColor &color, int size = 16);
QIcon lock(const QColor &color, int size = 16);

// Writes a chevron PNG once and returns its path, for stylesheets, which can
// only reference files. Fusion draws no arrow on a styled QComboBox or spin box.
QString chevronFile(bool up, const QColor &color);

} // namespace Icons
