#pragma once
#include <QDialog>
#include <functional>

#include "sync.h"

class Database;
class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QThread;

// Rewrites any password the Python edition left in plain text in the registry
// through DPAPI. Runs at every startup so the plaintext goes away even if the
// sync dialog is never opened.
void migrateStoredPasswords();

class SyncDialog : public QDialog {
    Q_OBJECT
public:
    explicit SyncDialog(Database *db, QWidget *parent = nullptr);

    void reject() override;  // Stays open while a worker is running.

signals:
    void dataReplaced();     // Restore swapped the data out from under the app.

private:
    void buildUi();
    void loadSettings();
    void onSave();
    void onSync();
    void onRestore();
    bool readForm(FirebaseConfig *config, QString *syncPassword);

    // Runs work on a worker thread, then done on the UI thread.
    void startWork(const std::function<void()> &work, const std::function<void()> &done);
    void setBusy(bool busy);
    void setStatus(const QString &text);
    void startPush(bool allowKeyChange);
    void finishPush();
    void offerCloudDeletion();
    void finishRestore();
    void showFailure();

    Database *m_db;
    QThread *m_thread = nullptr;

    FirebaseConfig m_config;
    QString m_syncPassword;
    Sync::PushResult m_pushResult;
    Sync::RestoreResult m_restoreResult;
    QString m_error;

    QLineEdit *m_apiKey;
    QLineEdit *m_projectId;
    QLineEdit *m_bucket;
    QLineEdit *m_email;
    QLineEdit *m_firebasePassword;
    QLineEdit *m_syncPasswordEdit;
    QCheckBox *m_remember;
    QLabel *m_status;
    QProgressBar *m_progress;
    QPushButton *m_syncButton;
    QPushButton *m_restoreButton;
};
