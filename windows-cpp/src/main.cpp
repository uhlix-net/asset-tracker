#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <exception>

#include <windows.h>
#include <shobjidl.h>

#include "config.h"
#include "database.h"
#include "mainwindow.h"
#include "syncdialog.h"
#include "theme.h"

namespace {

// A WIN32 app has no console, so Qt warnings and crashes land here. Kept out of
// %APPDATA%\AssetTracker because restore replaces that folder. Never log secrets.
void logMessage(QtMsgType, const QMessageLogContext &, const QString &message) {
    const QString dir = QDir::fromNativeSeparators(qEnvironmentVariable("LOCALAPPDATA"))
                        + "/AssetTracker";
    QDir().mkpath(dir);
    QFile file(dir + "/debug.log");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        file.write(QString("%1 %2\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate),
                                          message).toUtf8());
    }
}

void onTerminate() {
    try {
        std::rethrow_exception(std::current_exception());
    } catch (const std::exception &e) {
        qCritical("Unhandled exception: %s", e.what());
    } catch (...) {
        qCritical("Unhandled unknown exception");
    }
    std::abort();
}

} // namespace

int main(int argc, char *argv[]) {
    // Shows our icon on the taskbar and groups the window as its own app.
    SetCurrentProcessExplicitAppUserModelID(L"uhlix.net.AssetTracker");

    qInstallMessageHandler(logMessage);
    std::set_terminate(onTerminate);

    QApplication app(argc, argv);
    app.setApplicationName(Config::kAppName);
    app.setApplicationVersion(Config::kAppVersion);
    Theme::apply(app);
    app.setWindowIcon(QIcon(":/app.ico"));

    try {
        migrateStoredPasswords();
        Database db(Config::dbPath());
        MainWindow window(&db);
        window.show();
        return app.exec();
    } catch (const std::exception &e) {
        QMessageBox::critical(nullptr, Config::kAppName,
                              QString("Could not start:\n%1").arg(e.what()));
        return 1;
    }
}
