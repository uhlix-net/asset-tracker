#include "icons.h"

#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStandardPaths>

namespace {

// Draws into a square of edge s. Coordinates are fractions of s.
using DrawFn = void (*)(QPainter &, qreal);

QHash<QString, QIcon> g_cache;

// The cache holds pixmaps, which must not outlive QApplication.
void releaseCache() { g_cache.clear(); }

QPen linePen(const QColor &color, qreal s) {
    QPen pen(color, s * 0.085);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}

QIcon makeIcon(const char *key, const QColor &color, int size, DrawFn draw) {
    const QString cacheKey = QString("%1@%2/%3").arg(key).arg(size).arg(color.name(QColor::HexArgb));
    const auto cached = g_cache.constFind(cacheKey);
    if (cached != g_cache.constEnd()) return *cached;

    static bool cleanupRegistered = false;
    if (!cleanupRegistered) {
        qAddPostRoutine(releaseCache);
        cleanupRegistered = true;
    }

    QIcon icon;
    for (int scale : {1, 2, 3}) {
        const int px = size * scale;
        QPixmap pixmap(px, px);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(linePen(color, px));
        p.setBrush(Qt::NoBrush);
        draw(p, px);
        p.end();
        icon.addPixmap(pixmap);
    }
    g_cache.insert(cacheKey, icon);
    return icon;
}

void drawAdd(QPainter &p, qreal s) {
    p.drawLine(QPointF(s * 0.5, s * 0.2), QPointF(s * 0.5, s * 0.8));
    p.drawLine(QPointF(s * 0.2, s * 0.5), QPointF(s * 0.8, s * 0.5));
}

void drawEdit(QPainter &p, qreal s) {
    QPainterPath pencil;
    pencil.moveTo(s * 0.68, s * 0.14);
    pencil.lineTo(s * 0.86, s * 0.32);
    pencil.lineTo(s * 0.36, s * 0.82);
    pencil.lineTo(s * 0.14, s * 0.86);
    pencil.lineTo(s * 0.18, s * 0.64);
    pencil.closeSubpath();
    p.drawPath(pencil);
    p.drawLine(QPointF(s * 0.58, s * 0.24), QPointF(s * 0.76, s * 0.42));
}

void drawDuplicate(QPainter &p, qreal s) {
    p.drawRoundedRect(QRectF(s * 0.32, s * 0.32, s * 0.52, s * 0.52), s * 0.08, s * 0.08);
    QPainterPath back;
    back.moveTo(s * 0.68, s * 0.2);
    back.lineTo(s * 0.68, s * 0.16);
    back.lineTo(s * 0.16, s * 0.16);
    back.lineTo(s * 0.16, s * 0.68);
    back.lineTo(s * 0.2, s * 0.68);
    p.drawPath(back);
}

void drawRemove(QPainter &p, qreal s) {
    p.drawLine(QPointF(s * 0.16, s * 0.26), QPointF(s * 0.84, s * 0.26));
    p.drawLine(QPointF(s * 0.4, s * 0.26), QPointF(s * 0.42, s * 0.14));
    p.drawLine(QPointF(s * 0.42, s * 0.14), QPointF(s * 0.58, s * 0.14));
    p.drawLine(QPointF(s * 0.58, s * 0.14), QPointF(s * 0.6, s * 0.26));
    QPainterPath bin;
    bin.moveTo(s * 0.24, s * 0.26);
    bin.lineTo(s * 0.3, s * 0.86);
    bin.lineTo(s * 0.7, s * 0.86);
    bin.lineTo(s * 0.76, s * 0.26);
    p.drawPath(bin);
    p.drawLine(QPointF(s * 0.43, s * 0.42), QPointF(s * 0.44, s * 0.7));
    p.drawLine(QPointF(s * 0.57, s * 0.42), QPointF(s * 0.56, s * 0.7));
}

void drawBackup(QPainter &p, qreal s) {
    p.drawRoundedRect(QRectF(s * 0.14, s * 0.18, s * 0.72, s * 0.18), s * 0.04, s * 0.04);
    QPainterPath box;
    box.moveTo(s * 0.2, s * 0.36);
    box.lineTo(s * 0.2, s * 0.84);
    box.lineTo(s * 0.8, s * 0.84);
    box.lineTo(s * 0.8, s * 0.36);
    p.drawPath(box);
    p.drawLine(QPointF(s * 0.5, s * 0.46), QPointF(s * 0.5, s * 0.72));
    p.drawLine(QPointF(s * 0.38, s * 0.6), QPointF(s * 0.5, s * 0.72));
    p.drawLine(QPointF(s * 0.62, s * 0.6), QPointF(s * 0.5, s * 0.72));
}

void drawPrint(QPainter &p, qreal s) {
    QPainterPath top;
    top.moveTo(s * 0.28, s * 0.38);
    top.lineTo(s * 0.28, s * 0.14);
    top.lineTo(s * 0.72, s * 0.14);
    top.lineTo(s * 0.72, s * 0.38);
    p.drawPath(top);
    QPainterPath body;
    body.moveTo(s * 0.28, s * 0.7);
    body.lineTo(s * 0.12, s * 0.7);
    body.lineTo(s * 0.12, s * 0.38);
    body.lineTo(s * 0.88, s * 0.38);
    body.lineTo(s * 0.88, s * 0.7);
    body.lineTo(s * 0.72, s * 0.7);
    p.drawPath(body);
    p.drawRect(QRectF(s * 0.28, s * 0.58, s * 0.44, s * 0.28));
}

void drawSearch(QPainter &p, qreal s) {
    p.drawEllipse(QRectF(s * 0.16, s * 0.16, s * 0.48, s * 0.48));
    p.drawLine(QPointF(s * 0.58, s * 0.58), QPointF(s * 0.84, s * 0.84));
}

void drawFolder(QPainter &p, qreal s) {
    QPainterPath folder;
    folder.moveTo(s * 0.12, s * 0.8);
    folder.lineTo(s * 0.12, s * 0.22);
    folder.lineTo(s * 0.4, s * 0.22);
    folder.lineTo(s * 0.48, s * 0.32);
    folder.lineTo(s * 0.88, s * 0.32);
    folder.lineTo(s * 0.88, s * 0.8);
    folder.closeSubpath();
    p.drawPath(folder);
}

void drawPhoto(QPainter &p, qreal s) {
    p.drawRoundedRect(QRectF(s * 0.12, s * 0.2, s * 0.76, s * 0.6), s * 0.06, s * 0.06);
    QPainterPath hills;
    hills.moveTo(s * 0.14, s * 0.72);
    hills.lineTo(s * 0.38, s * 0.48);
    hills.lineTo(s * 0.56, s * 0.66);
    hills.lineTo(s * 0.66, s * 0.56);
    hills.lineTo(s * 0.86, s * 0.76);
    p.drawPath(hills);
    p.drawEllipse(QPointF(s * 0.66, s * 0.36), s * 0.06, s * 0.06);
}

void drawLock(QPainter &p, qreal s) {
    p.drawRoundedRect(QRectF(s * 0.2, s * 0.44, s * 0.6, s * 0.42), s * 0.06, s * 0.06);
    QPainterPath shackle;
    shackle.moveTo(s * 0.32, s * 0.44);
    shackle.lineTo(s * 0.32, s * 0.32);
    shackle.arcTo(QRectF(s * 0.32, s * 0.14, s * 0.36, s * 0.36), 180, -180);
    shackle.lineTo(s * 0.68, s * 0.44);
    p.drawPath(shackle);
}

} // namespace

QIcon Icons::add(const QColor &c, int size)       { return makeIcon("add", c, size, drawAdd); }
QIcon Icons::edit(const QColor &c, int size)      { return makeIcon("edit", c, size, drawEdit); }
QIcon Icons::duplicate(const QColor &c, int size) { return makeIcon("duplicate", c, size, drawDuplicate); }
QIcon Icons::remove(const QColor &c, int size)    { return makeIcon("remove", c, size, drawRemove); }
QIcon Icons::backup(const QColor &c, int size)    { return makeIcon("backup", c, size, drawBackup); }
QIcon Icons::print(const QColor &c, int size)     { return makeIcon("print", c, size, drawPrint); }
QIcon Icons::search(const QColor &c, int size)    { return makeIcon("search", c, size, drawSearch); }
QIcon Icons::folder(const QColor &c, int size)    { return makeIcon("folder", c, size, drawFolder); }
QIcon Icons::photo(const QColor &c, int size)     { return makeIcon("photo", c, size, drawPhoto); }
QIcon Icons::lock(const QColor &c, int size)      { return makeIcon("lock", c, size, drawLock); }

QString Icons::chevronFile(bool up, const QColor &color) {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + "/AssetTracker";
    QDir().mkpath(dir);
    const QString path = QString("%1/chevron-%2-%3.png").arg(dir, up ? "up" : "down",
                                                            color.name().mid(1));
    // 3x, so it stays sharp at any display scaling once Qt shrinks it.
    constexpr int px = 36;
    QPixmap pixmap(px, px);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, px * 0.11);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    const qreal top = up ? 0.62 : 0.38, tip = up ? 0.38 : 0.62;
    p.drawPolyline(QPolygonF({QPointF(px * 0.28, px * top), QPointF(px * 0.5, px * tip),
                              QPointF(px * 0.72, px * top)}));
    p.end();
    pixmap.save(path, "PNG");
    return path;
}
