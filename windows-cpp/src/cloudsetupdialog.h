#pragma once
#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QStackedWidget;

// The online sync setup, shown at first start and from Preferences: the Firebase
// project (unless the build locks it), then create an account or sign in, then the
// sync password. Settings are saved as each step succeeds.
class CloudSetupDialog : public QDialog {
    Q_OBJECT
public:
    enum class StartAt { Welcome, Account };

    explicit CloudSetupDialog(StartAt start = StartAt::Welcome, QWidget *parent = nullptr);

private:
    enum Page { WelcomePage, ProjectPage, AccountPage, SyncPasswordPage };

    QWidget *buildWelcome();
    QWidget *buildProject();
    QWidget *buildAccount();
    QWidget *buildSyncPassword();

    void showPage(int page);
    void onNext();
    void onBack();
    void onCancel();
    bool finishProject();
    bool finishAccount();
    bool finishSyncPassword();
    void sendReset();
    void updateAccountMode();

    QStackedWidget *m_pages;
    QPushButton *m_back;
    QPushButton *m_next;
    QPushButton *m_cancel;
    int m_firstPage;
    bool m_newAccount = true;

    QLineEdit *m_apiKey;
    QLineEdit *m_projectId;
    QLineEdit *m_bucket;

    QRadioButton *m_createRadio;
    QRadioButton *m_signInRadio;
    QLineEdit *m_email;
    QLineEdit *m_password;
    QLineEdit *m_confirm;
    QLabel *m_confirmLabel;
    QLabel *m_forgot;

    QLabel *m_syncIntro;
    QLineEdit *m_syncPassword;
    QLineEdit *m_syncConfirm;
    QLabel *m_syncConfirmLabel;
    QWidget *m_syncWarning;
    QCheckBox *m_remember;
};
