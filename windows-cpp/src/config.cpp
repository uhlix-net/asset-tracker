#include "config.h"

#include <QDir>

namespace {
QString g_testRoot;
}

void Config::setTestRoot(const QString &dir) { g_testRoot = dir; }
bool Config::isTestRun() { return !g_testRoot.isEmpty(); }

QString Config::dataDir() {
    const QString base = g_testRoot.isEmpty()
        ? QDir::fromNativeSeparators(qEnvironmentVariable("APPDATA"))
        : g_testRoot;
    return base + "/" + kAppName;
}

QString Config::dbDir() { return dataDir() + "/db"; }
QString Config::dbPath() { return dbDir() + "/tracker.db"; }
QString Config::assetsDir() { return dataDir() + "/assets"; }

std::unique_ptr<QSettings> Config::openSettings() {
    if (!g_testRoot.isEmpty())
        return std::make_unique<QSettings>(g_testRoot + "/settings.ini", QSettings::IniFormat);
    return std::make_unique<QSettings>(kAppName, kAppName);
}

const QStringList &Config::categories() {
    static const QStringList list = {
        "Living Room", "Bedroom", "Kitchen", "Dining Room", "Bathroom",
        "Office / Study", "Garage", "Basement / Attic", "Outdoor / Patio",
        "Firearms", "Personal Effects", "Other",
    };
    return list;
}

const QStringList &Config::imageExtensions() {
    static const QStringList list = {
        ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp", ".tiff", ".tif", ".ico",
    };
    return list;
}

const QStringList &Config::receiptExtensions() {
    static const QStringList list = {".pdf", ".png", ".jpg", ".jpeg", ".tiff", ".tif"};
    return list;
}

QString Config::filterFor(const QStringList &extensions) {
    QStringList globs;
    for (const QString &ext : extensions)
        globs << "*" + ext;
    globs.sort();
    return globs.join(' ');
}
