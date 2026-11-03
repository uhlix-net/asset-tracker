#pragma once
#include <QDialog>

class Database;
class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;

// File → Preferences. Today it holds online sync: the service connection, the
// account, the sync password, and deleting what is stored online.
class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(Database *db, QWidget *parent = nullptr);

signals:
    void syncRequested(bool rekey);     // Run a sync, or a full re-upload (new sync password).

private:
    QWidget *buildSyncTab();
    void refresh();
    void saveProject();
    void setUpAccount();
    void changePassword();
    void sendReset();
    void signOut();
    void toggleRemember(bool remember);
    void changeSyncPassword();
    void deleteOnlineData();
    void deleteAccount();
    bool confirmDeletion(const QString &title, const QString &what);

    Database *m_db;

    QGroupBox *m_projectGroup;
    QLineEdit *m_apiKey;
    QLineEdit *m_projectId;
    QLineEdit *m_bucket;

    QLabel *m_accountStatus;
    QPushButton *m_setUp;
    QPushButton *m_changePassword;
    QPushButton *m_reset;
    QPushButton *m_signOut;

    QCheckBox *m_remember;
    QPushButton *m_changeSync;

    QPushButton *m_deleteData;
    QPushButton *m_deleteAccount;
};
