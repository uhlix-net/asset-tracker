#include "busy.h"

#include <QEventLoop>
#include <QFutureWatcher>
#include <QProgressDialog>
#include <QtConcurrent>
#include <exception>

void runBusy(QWidget *parent, const QString &label, const std::function<void()> &work) {
    QProgressDialog dialog(label, QString(), 0, 0, parent);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setMinimumDuration(0);
    dialog.setAutoClose(false);
    dialog.setAutoReset(false);
    dialog.setWindowFlag(Qt::WindowCloseButtonHint, false);

    std::exception_ptr error;
    QFutureWatcher<void> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(QtConcurrent::run([&] {
        try {
            work();
        } catch (...) {
            error = std::current_exception();
        }
    }));
    dialog.show();
    loop.exec();
    dialog.close();

    if (error) std::rethrow_exception(error);
}
