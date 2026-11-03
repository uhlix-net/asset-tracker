#pragma once
#include <QSettings>
#include <QString>
#include <QStringList>
#include <memory>

// Paths and fixed lists. These must stay in step with what earlier versions
// wrote: the same %APPDATA%\AssetTracker tree and the same registry key, so an
// install upgrades in place.
namespace Config {

inline constexpr char kAppName[] = "AssetTracker";
inline constexpr char kAppVersion[] = ASSET_TRACKER_VERSION;

QString dataDir();
QString dbDir();
QString dbPath();
QString assetsDir();

// HKCU\Software\AssetTracker\AssetTracker, or an ini file under a test root.
std::unique_ptr<QSettings> openSettings();

// Moves all data and settings under dir, so tests never touch the real ones.
void setTestRoot(const QString &dir);
bool isTestRun();

const QStringList &categories();
const QStringList &imageExtensions();    // ".png", ...
const QStringList &receiptExtensions();

// File-dialog filter text, e.g. "*.jpg *.png".
QString filterFor(const QStringList &extensions);

} // namespace Config
