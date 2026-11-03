#pragma once
#include <QDialog>
#include <functional>

#include "sync.h"

class Database;
class QLabel;
class QProgressBar;
class QPushButton;
class QThread;

// Runs one cloud operation with the saved settings and shows its progress.
// Setup and account management live in CloudSetupDialog and PreferencesDialog.
class SyncDialog : public QDialog {
    Q_OBJECT
public:
    enum class Mode {
        Sync,       // Upload what changed.
        Restore,    // Replace this PC's data with the cloud copy.
        Rekey,      // Upload everything again under a new sync password.
    };

    SyncDialog(Database *db, Mode mode, QWidget *parent = nullptr);

    void reject() override;  // Stays open while a worker is running.

signals:
    void dataReplaced();     // Restore swapped the data out from under the app.

protected:
    void showEvent(QShowEvent *event) override;

private:
    void start();
    void startWork(const std::function<void()> &work, const std::function<void()> &done);
    void setStatus(const QString &text);
    void finishWithError(const QString &message);
    void startPush(bool allowKeyChange);
    void finishPush();
    void offerCloudDeletion();
    void startRestore();
    void finishRestore();

    Database *m_db;
    Mode m_mode;
    bool m_started = false;
    QThread *m_thread = nullptr;

    FirebaseConfig m_config;
    QString m_syncPassword;
    Sync::PushResult m_pushResult;
    Sync::RestoreResult m_restoreResult;
    QString m_error;

    QLabel *m_heading;
    QLabel *m_status;
    QProgressBar *m_progress;
    QPushButton *m_close;
};
