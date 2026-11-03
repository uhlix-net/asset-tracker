#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <exception>

#include <windows.h>
#include <dbghelp.h>
#include <shobjidl.h>
#include <cstdio>

#include "config.h"
#include "database.h"
#include "mainwindow.h"
#include "cloudconfig.h"
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

// Where crash dumps go: %LOCALAPPDATA%\AssetTracker. Worked out at start, because
// the crash handler must not allocate or call into Qt.
wchar_t g_logDir[MAX_PATH] = {};

void appendRaw(const char *text) {
    wchar_t path[MAX_PATH + 32];
    swprintf(path, sizeof(path) / sizeof(path[0]), L"%ls\\debug.log", g_logDir);
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
    CloseHandle(file);
}

// Any thread's unhandled crash: write a minidump beside debug.log and one line saying
// where it is, then let Windows carry on with its normal crash handling.
LONG WINAPI onCrash(EXCEPTION_POINTERS *info) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    wchar_t dump[MAX_PATH + 64];
    swprintf(dump, sizeof(dump) / sizeof(dump[0]), L"%ls\\crash-%04d%02d%02d-%02d%02d%02d.dmp",
             g_logDir, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

    HANDLE file = CreateFileW(dump, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION exception{GetCurrentThreadId(), info, FALSE};
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                          static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory
                                                     | MiniDumpWithThreadInfo),
                          &exception, nullptr, nullptr);
        CloseHandle(file);
    }

    char line[MAX_PATH * 3];
    snprintf(line, sizeof(line), "%04d-%02d-%02dT%02d:%02d:%02d CRASH: exception 0x%08lX at %p "
             "on thread %lu. Dump: %ls\n", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
             now.wSecond, info->ExceptionRecord->ExceptionCode,
             info->ExceptionRecord->ExceptionAddress, GetCurrentThreadId(), dump);
    appendRaw(line);
    return EXCEPTION_CONTINUE_SEARCH;
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

    const std::wstring logDir = QDir::toNativeSeparators(
        QDir::fromNativeSeparators(qEnvironmentVariable("LOCALAPPDATA")) + "/AssetTracker")
        .toStdWString();
    QDir().mkpath(QString::fromStdWString(logDir));
    wcsncpy_s(g_logDir, logDir.c_str(), _TRUNCATE);
    SetUnhandledExceptionFilter(onCrash);

    qInstallMessageHandler(logMessage);
    std::set_terminate(onTerminate);
    qInfo("Asset Tracker %s starting", Config::kAppVersion);

    QApplication app(argc, argv);
    app.setApplicationName(Config::kAppName);
    app.setApplicationVersion(Config::kAppVersion);
    Theme::apply(app);
    app.setWindowIcon(QIcon(":/app.ico"));

    try {
        CloudConfig::migrateStoredPasswords();
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
