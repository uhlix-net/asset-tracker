#include "preferencesdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "busy.h"
#include "cloudconfig.h"
#include "cloudsetupdialog.h"
#include "database.h"
#include "sync.h"
#include "theme.h"

namespace {

constexpr int kMinPasswordLength = 6;
constexpr int kMinSyncPasswordLength = 10;

QLabel *hint(const QString &text) {
    auto *label = new QLabel(text);
    label->setObjectName("hint");
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    return label;
}

QHBoxLayout *buttonRow(std::initializer_list<QPushButton *> buttons) {
    auto *row = new QHBoxLayout;
    for (QPushButton *button : buttons)
        row->addWidget(button);
    row->addStretch();
    return row;
}

QString askPassword(QWidget *parent, const QString &title, const QString &label, bool *ok) {
    return QInputDialog::getText(parent, title, label, QLineEdit::Password, QString(), ok);
}

void ignoreProgress(const QString &) {}

} // namespace

PreferencesDialog::PreferencesDialog(Database *db, QWidget *parent)
    : QDialog(parent)
    , m_db(db)
{
    setWindowTitle("Preferences");
    setMinimumWidth(600);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    auto *tabs = new QTabWidget;
    tabs->addTab(buildSyncTab(), "Online Sync");
    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttons);
    refresh();
}

QWidget *PreferencesDialog::buildSyncTab() {
    auto *tab = new QWidget;
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    // Service connection: shared by every user, so hidden when the build locks it.
    m_projectGroup = new QGroupBox("Online service");
    auto *projectLayout = new QVBoxLayout(m_projectGroup);
    const CloudConfig::Project project = CloudConfig::project();
    m_apiKey = new QLineEdit(project.apiKey);
    m_projectId = new QLineEdit(project.projectId);
    m_bucket = new QLineEdit(project.bucket);
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addRow("API key", m_apiKey);
    form->addRow("Project ID", m_projectId);
    form->addRow("Storage bucket", m_bucket);
    projectLayout->addLayout(form);
    auto *saveProject = new QPushButton("Save Connection");
    connect(saveProject, &QPushButton::clicked, this, &PreferencesDialog::saveProject);
    projectLayout->addLayout(buttonRow({saveProject}));
    m_projectGroup->setVisible(!CloudConfig::isLocked());
    layout->addWidget(m_projectGroup);

    auto *accountGroup = new QGroupBox("Account");
    auto *accountLayout = new QVBoxLayout(accountGroup);
    m_accountStatus = new QLabel;
    m_accountStatus->setWordWrap(true);
    accountLayout->addWidget(m_accountStatus);
    m_setUp = new QPushButton;
    m_changePassword = new QPushButton("Change Password...");
    m_reset = new QPushButton("Email Me a Reset Link");
    m_signOut = new QPushButton("Sign Out");
    connect(m_setUp, &QPushButton::clicked, this, &PreferencesDialog::setUpAccount);
    connect(m_changePassword, &QPushButton::clicked, this, &PreferencesDialog::changePassword);
    connect(m_reset, &QPushButton::clicked, this, &PreferencesDialog::sendReset);
    connect(m_signOut, &QPushButton::clicked, this, &PreferencesDialog::signOut);
    accountLayout->addLayout(buttonRow({m_setUp}));
    accountLayout->addLayout(buttonRow({m_changePassword, m_reset, m_signOut}));
    layout->addWidget(accountGroup);

    auto *syncGroup = new QGroupBox("Sync password");
    auto *syncLayout = new QVBoxLayout(syncGroup);
    m_remember = new QCheckBox("Remember on this PC (stored encrypted for your Windows account)");
    connect(m_remember, &QCheckBox::clicked, this, &PreferencesDialog::toggleRemember);
    syncLayout->addWidget(m_remember);
    m_changeSync = new QPushButton("Change Sync Password...");
    connect(m_changeSync, &QPushButton::clicked, this, &PreferencesDialog::changeSyncPassword);
    syncLayout->addLayout(buttonRow({m_changeSync}));
    layout->addWidget(syncGroup);

    auto *dangerGroup = new QGroupBox("Delete online data");
    auto *dangerLayout = new QVBoxLayout(dangerGroup);
    dangerLayout->addWidget(hint(
        "Removes your data from the online service. The items, photos and receipts on this "
        "PC are not affected. <b>This cannot be undone.</b>"));
    m_deleteData = new QPushButton("Delete Online Data...");
    m_deleteAccount = new QPushButton("Delete Account...");
    for (QPushButton *button : {m_deleteData, m_deleteAccount})
        button->setProperty("danger", true);
    connect(m_deleteData, &QPushButton::clicked, this, &PreferencesDialog::deleteOnlineData);
    connect(m_deleteAccount, &QPushButton::clicked, this, &PreferencesDialog::deleteAccount);
    dangerLayout->addLayout(buttonRow({m_deleteData, m_deleteAccount}));
    layout->addWidget(dangerGroup);

    layout->addStretch();
    return tab;
}

void PreferencesDialog::refresh() {
    const bool signedIn = CloudConfig::hasAccount();
    m_accountStatus->setText(signedIn
        ? QString("Signed in as <b>%1</b>.").arg(CloudConfig::email().toHtmlEscaped())
        : QStringLiteral("Online sync is not set up on this PC."));
    m_setUp->setText(signedIn ? "Switch Account..." : "Set Up Online Sync...");
    Theme::markPrimary(m_setUp, !signedIn);
    for (QPushButton *button : {m_changePassword, m_signOut, m_changeSync, m_deleteData,
                                m_deleteAccount})
        button->setEnabled(signedIn);
    m_remember->setEnabled(signedIn);
    m_remember->setChecked(CloudConfig::rememberSyncPassword());
}

void PreferencesDialog::saveProject() {
    const CloudConfig::Project project{m_apiKey->text().trimmed(), m_projectId->text().trimmed(),
                                       m_bucket->text().trimmed()};
    if (!project.isComplete()) {
        QMessageBox::warning(this, windowTitle(), "Fill in all three connection details.");
        return;
    }
    CloudConfig::saveProject(project);
    QMessageBox::information(this, windowTitle(), "Connection saved.");
}

void PreferencesDialog::setUpAccount() {
    CloudSetupDialog dialog(CloudSetupDialog::StartAt::Account, this);
    if (dialog.exec() == QDialog::Accepted) {
        // Fingerprints belong to the old account; the next sync checks everything.
        m_db->saveSyncState(SyncState());
    }
    refresh();
}

void PreferencesDialog::changePassword() {
    bool ok = false;
    const QString password = askPassword(this, "Change Password",
        QString("New account password (at least %1 characters):").arg(kMinPasswordLength), &ok);
    if (!ok) return;
    const QString confirm = askPassword(this, "Change Password", "Type it again:", &ok);
    if (!ok) return;
    if (password.length() < kMinPasswordLength || password != confirm) {
        QMessageBox::warning(this, windowTitle(), password != confirm
            ? QStringLiteral("The two passwords don't match.")
            : QString("Use at least %1 characters.").arg(kMinPasswordLength));
        return;
    }
    try {
        const FirebaseConfig config = CloudConfig::firebaseConfig();
        runBusy(this, "Changing password…", [&] { Sync::changeAccountPassword(config, password); });
        CloudConfig::saveAccount(CloudConfig::email(), password);
        QMessageBox::information(this, windowTitle(), "Your account password was changed.");
    } catch (const std::exception &e) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8(e.what()));
    }
}

void PreferencesDialog::sendReset() {
    bool ok = false;
    const QString email = QInputDialog::getText(this, "Reset Password",
        "Email address of the account:", QLineEdit::Normal, CloudConfig::email(), &ok).trimmed();
    if (!ok || email.isEmpty()) return;
    try {
        const FirebaseConfig config = CloudConfig::firebaseConfig();
        runBusy(this, "Sending…", [&] { Sync::sendPasswordReset(config, email); });
        QMessageBox::information(this, windowTitle(),
            QString("If %1 has an account, a password reset email is on its way. After "
                    "resetting, use Switch Account to sign in with the new password.").arg(email));
    } catch (const std::exception &e) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8(e.what()));
    }
}

void PreferencesDialog::signOut() {
    const auto answer = QMessageBox::question(this, "Sign Out",
        "Sign out of online sync on this PC? Your online copy and the data on this PC are not "
        "changed. You'll need your email, password and sync password to sign in again.");
    if (answer != QMessageBox::Yes) return;
    CloudConfig::signOut();
    m_db->saveSyncState(SyncState());
    refresh();
}

void PreferencesDialog::toggleRemember(bool remember) {
    if (!remember) {
        CloudConfig::saveSyncPassword(QString(), false);
        return;
    }
    bool ok = false;
    const QString password = askPassword(this, "Remember Sync Password",
                                         "Enter your sync password:", &ok);
    if (!ok || password.isEmpty()) {
        m_remember->setChecked(false);
        return;
    }
    Sync::KeyCheck check = Sync::KeyCheck::NoCloudData;
    try {
        const FirebaseConfig config = CloudConfig::firebaseConfig();
        runBusy(this, "Checking…", [&] { check = Sync::checkSyncPassword(config, password); });
    } catch (const std::exception &e) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8(e.what()));
        m_remember->setChecked(false);
        return;
    }
    if (check == Sync::KeyCheck::DoesNotMatch) {
        QMessageBox::warning(this, windowTitle(),
                             "That sync password doesn't open your online copy, so it wasn't saved.");
        m_remember->setChecked(false);
        return;
    }
    CloudConfig::saveSyncPassword(password, true);
}

void PreferencesDialog::changeSyncPassword() {
    const auto answer = QMessageBox::warning(this, "Change Sync Password",
        "Changing your sync password uploads everything on this PC again, locked with the new "
        "password. Items that are only in the cloud stay locked with the old one.\n\n"
        "The Android app will need the new password too. Continue?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    bool ok = false;
    const QString password = askPassword(this, "Change Sync Password",
        QString("New sync password (at least %1 characters):").arg(kMinSyncPasswordLength), &ok);
    if (!ok) return;
    const QString confirm = askPassword(this, "Change Sync Password", "Type it again:", &ok);
    if (!ok) return;
    if (password.length() < kMinSyncPasswordLength || password != confirm) {
        QMessageBox::warning(this, windowTitle(), password != confirm
            ? QStringLiteral("The two sync passwords don't match.")
            : QString("Use at least %1 characters.").arg(kMinSyncPasswordLength));
        return;
    }
    QMessageBox::information(this, windowTitle(),
        "Write your new sync password down and keep it safe. It can't be recovered.");

    CloudConfig::saveSyncPassword(password, true);
    m_db->saveSyncState(SyncState());
    refresh();
    emit syncRequested(true);
}

// Two steps on purpose: an explanation the user has to read, then typing DELETE.
bool PreferencesDialog::confirmDeletion(const QString &title, const QString &what) {
    QMessageBox box(QMessageBox::Warning, title,
                    what + "\n\nThis is permanent and cannot be undone. Nobody, including "
                           "Uhlix, can bring it back.",
                    QMessageBox::Cancel, this);
    QPushButton *proceed = box.addButton("Continue", QMessageBox::DestructiveRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != proceed) return false;

    bool ok = false;
    const QString typed = QInputDialog::getText(this, title,
        "To confirm, type DELETE in capital letters:", QLineEdit::Normal, QString(), &ok);
    if (!ok) return false;
    if (typed.trimmed() != "DELETE") {
        QMessageBox::information(this, title, "Nothing was deleted.");
        return false;
    }
    return true;
}

void PreferencesDialog::deleteOnlineData() {
    if (!confirmDeletion("Delete Online Data",
            "This deletes every item, photo and receipt in your online copy. Your account "
            "stays, and the data on this PC is not touched.\n\nThe Android app will show "
            "nothing until you sync again."))
        return;
    try {
        const FirebaseConfig config = CloudConfig::firebaseConfig();
        runBusy(this, "Deleting your online data…",
                [&] { Sync::deleteAllCloudData(config, &ignoreProgress); });
        m_db->saveSyncState(SyncState());
        QMessageBox::information(this, windowTitle(), "Your online data was deleted.");
    } catch (const std::exception &e) {
        QMessageBox::critical(this, windowTitle(),
            QString("Deleting stopped part-way. Try again.\n\n%1").arg(e.what()));
    }
}

void PreferencesDialog::deleteAccount() {
    if (!confirmDeletion("Delete Account",
            "This deletes your online copy, every item, photo and receipt in it, and then your "
            "online sync account itself. The data on this PC is not touched.\n\nTo use online "
            "sync again you would need to create a new account."))
        return;
    try {
        const FirebaseConfig config = CloudConfig::firebaseConfig();
        runBusy(this, "Deleting your account…",
                [&] { Sync::deleteAccount(config, &ignoreProgress); });
        CloudConfig::signOut();
        m_db->saveSyncState(SyncState());
        refresh();
        QMessageBox::information(this, windowTitle(),
                                 "Your online data and account were deleted.");
    } catch (const std::exception &e) {
        QMessageBox::critical(this, windowTitle(),
            QString("Deleting stopped part-way. Try again.\n\n%1").arg(e.what()));
    }
}
