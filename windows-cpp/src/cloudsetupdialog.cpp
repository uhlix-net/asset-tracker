#include "cloudsetupdialog.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "busy.h"
#include "cloudconfig.h"
#include "icons.h"
#include "sync.h"
#include "theme.h"

namespace {

constexpr int kMinPasswordLength = 6;      // Firebase's own minimum.
constexpr int kMinSyncPasswordLength = 10;

QLabel *heading(const QString &text) {
    auto *label = new QLabel(text);
    label->setObjectName("title");
    label->setWordWrap(true);
    return label;
}

QLabel *paragraph(const QString &text, const char *role = nullptr) {
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    if (role) label->setObjectName(role);
    return label;
}

QLineEdit *passwordField() {
    auto *field = new QLineEdit;
    field->setEchoMode(QLineEdit::Password);
    return field;
}

QFormLayout *formLayout() {
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(8);
    return form;
}

QWidget *warningNote(const QString &text) {
    auto *icon = new QLabel;
    icon->setPixmap(Icons::lock(Theme::kDanger, 14).pixmap(14, 14));
    icon->setAlignment(Qt::AlignTop);
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->addWidget(icon);
    layout->addWidget(label, 1);
    return row;
}

QString reason(const std::exception &e) { return QString::fromUtf8(e.what()); }

} // namespace

CloudSetupDialog::CloudSetupDialog(StartAt start, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Set Up Online Sync");
    setMinimumSize(520, 400);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);

    m_pages = new QStackedWidget;
    m_pages->addWidget(buildWelcome());
    m_pages->addWidget(buildProject());
    m_pages->addWidget(buildAccount());
    m_pages->addWidget(buildSyncPassword());
    layout->addWidget(m_pages, 1);

    auto *buttons = new QHBoxLayout;
    m_back = new QPushButton("Back");
    m_cancel = new QPushButton("Cancel");
    m_next = new QPushButton("Next");
    Theme::markPrimary(m_next);
    connect(m_back, &QPushButton::clicked, this, &CloudSetupDialog::onBack);
    connect(m_next, &QPushButton::clicked, this, &CloudSetupDialog::onNext);
    connect(m_cancel, &QPushButton::clicked, this, &CloudSetupDialog::onCancel);
    buttons->addWidget(m_back);
    buttons->addStretch();
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_next);
    layout->addLayout(buttons);

    m_firstPage = start == StartAt::Welcome ? WelcomePage
                  : CloudConfig::isLocked() ? AccountPage : ProjectPage;
    showPage(m_firstPage);
}

QWidget *CloudSetupDialog::buildWelcome() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(heading("Keep a safe copy of your inventory online"));
    layout->addSpacing(6);
    layout->addWidget(paragraph(
        "Online sync keeps an encrypted copy of your items, photos and receipts on the internet. "
        "If this computer is ever lost, stolen or damaged, you can bring everything back on a "
        "new one."));
    layout->addWidget(paragraph(
        "Everything is locked with a <b>sync password</b> that only you know before it leaves "
        "this computer. Nobody else can open it, not even Uhlix."));
    layout->addWidget(paragraph(
        "It takes about two minutes. You can also do it later from "
        "<b>File → Preferences</b>.", "hint"));
    layout->addStretch();
    return page;
}

QWidget *CloudSetupDialog::buildProject() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(heading("Online sync service"));
    layout->addWidget(paragraph(
        "Enter the connection details for the Asset Tracker online service. You only need "
        "to do this once.", "hint"));
    layout->addSpacing(8);

    const CloudConfig::Project project = CloudConfig::project();
    m_apiKey = new QLineEdit(project.apiKey);
    m_projectId = new QLineEdit(project.projectId);
    m_bucket = new QLineEdit(project.bucket);
    m_bucket->setPlaceholderText("your-project-id.firebasestorage.app");

    auto *form = formLayout();
    form->addRow("API key", m_apiKey);
    form->addRow("Project ID", m_projectId);
    form->addRow("Storage bucket", m_bucket);
    layout->addLayout(form);
    layout->addStretch();
    return page;
}

QWidget *CloudSetupDialog::buildAccount() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(heading("Your online sync account"));
    layout->addSpacing(4);

    m_createRadio = new QRadioButton("I'm new: create an account");
    m_signInRadio = new QRadioButton("I already have an account: sign in");
    m_createRadio->setChecked(true);
    connect(m_createRadio, &QRadioButton::toggled, this, &CloudSetupDialog::updateAccountMode);
    layout->addWidget(m_createRadio);
    layout->addWidget(m_signInRadio);
    layout->addSpacing(8);

    m_email = new QLineEdit(CloudConfig::email());
    m_email->setPlaceholderText("you@example.com");
    m_password = passwordField();
    m_confirm = passwordField();
    m_confirmLabel = new QLabel("Confirm password");

    auto *form = formLayout();
    form->addRow("Email", m_email);
    form->addRow("Password", m_password);
    form->addRow(m_confirmLabel, m_confirm);
    layout->addLayout(form);

    m_forgot = paragraph("<a href=\"reset\">Forgot your password?</a>");
    m_forgot->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(m_forgot, &QLabel::linkActivated, this, &CloudSetupDialog::sendReset);
    layout->addWidget(m_forgot, 0, Qt::AlignRight);

    layout->addWidget(paragraph(
        "This password signs you in to the online service. It is different from your sync "
        "password, which you choose next.", "hint"));
    layout->addStretch();

    if (CloudConfig::hasAccount()) m_signInRadio->setChecked(true);
    updateAccountMode();
    return page;
}

QWidget *CloudSetupDialog::buildSyncPassword() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(heading("Your sync password"));

    m_syncIntro = paragraph(QString());
    layout->addWidget(m_syncIntro);
    layout->addSpacing(4);

    m_syncPassword = passwordField();
    m_syncConfirm = passwordField();
    m_syncConfirmLabel = new QLabel("Confirm sync password");
    auto *form = formLayout();
    form->addRow("Sync password", m_syncPassword);
    form->addRow(m_syncConfirmLabel, m_syncConfirm);
    layout->addLayout(form);

    m_syncWarning = warningNote(
        "<b>Write it down and keep it safe.</b> It can't be reset or recovered by anyone. "
        "Without it your online copy can never be opened again.");
    layout->addWidget(m_syncWarning);

    m_remember = new QCheckBox("Remember on this PC (stored encrypted for your Windows account)");
    m_remember->setChecked(true);
    layout->addWidget(m_remember);
    layout->addStretch();
    return page;
}

void CloudSetupDialog::updateAccountMode() {
    const bool create = m_createRadio->isChecked();
    m_confirm->setVisible(create);
    m_confirmLabel->setVisible(create);
    m_forgot->setVisible(!create);
}

void CloudSetupDialog::showPage(int page) {
    m_pages->setCurrentIndex(page);
    m_back->setVisible(page != m_firstPage);
    m_cancel->setText(page == WelcomePage ? "Not Now" : "Cancel");
    m_next->setText(page == WelcomePage ? "Set Up Online Sync"
                    : page == SyncPasswordPage ? "Finish" : "Next");

    if (page == SyncPasswordPage) {
        m_syncIntro->setText(m_newAccount
            ? "Choose a sync password. It locks your data before it leaves this computer. "
              "Use something long and memorable, such as four unrelated words."
            : "Enter the sync password you chose when you first set up online sync.");
        m_syncConfirm->setVisible(m_newAccount);
        m_syncConfirmLabel->setVisible(m_newAccount);
        m_syncPassword->setFocus();
    }
}

void CloudSetupDialog::onBack() {
    int page = m_pages->currentIndex() - 1;
    if (page == ProjectPage && CloudConfig::isLocked()) --page;
    showPage(qMax(page, m_firstPage));
}

void CloudSetupDialog::onCancel() {
    // Declining the first-run prompt is remembered; Preferences can start it again.
    if (m_pages->currentIndex() == WelcomePage) CloudConfig::setSetupPrompted(true);
    reject();
}

void CloudSetupDialog::onNext() {
    switch (m_pages->currentIndex()) {
    case WelcomePage:
        showPage(CloudConfig::isLocked() ? AccountPage : ProjectPage);
        break;
    case ProjectPage:
        if (finishProject()) showPage(AccountPage);
        break;
    case AccountPage:
        if (finishAccount()) showPage(SyncPasswordPage);
        break;
    case SyncPasswordPage:
        if (finishSyncPassword()) {
            CloudConfig::setSetupPrompted(true);
            accept();
        }
        break;
    }
}

bool CloudSetupDialog::finishProject() {
    const CloudConfig::Project project{m_apiKey->text().trimmed(), m_projectId->text().trimmed(),
                                       m_bucket->text().trimmed()};
    if (!project.isComplete()) {
        QMessageBox::warning(this, windowTitle(), "Fill in all three connection details.");
        return false;
    }
    CloudConfig::saveProject(project);
    return true;
}

bool CloudSetupDialog::finishAccount() {
    const QString email = m_email->text().trimmed();
    const QString password = m_password->text();
    m_newAccount = m_createRadio->isChecked();

    if (email.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), "Enter your email address and a password.");
        return false;
    }
    if (m_newAccount) {
        if (password.length() < kMinPasswordLength) {
            QMessageBox::warning(this, windowTitle(),
                QString("Choose a password of at least %1 characters.").arg(kMinPasswordLength));
            return false;
        }
        if (password != m_confirm->text()) {
            QMessageBox::warning(this, windowTitle(), "The two passwords don't match.");
            return false;
        }
    }

    FirebaseConfig config = CloudConfig::firebaseConfig();
    config.email = email;
    config.password = password;
    try {
        runBusy(this, m_newAccount ? "Creating your account…" : "Signing in…", [&] {
            if (m_newAccount) Sync::createAccount(config);
            else Sync::signIn(config);
        });
    } catch (const std::exception &e) {
        QMessageBox::warning(this, windowTitle(), reason(e));
        return false;
    }
    CloudConfig::saveAccount(email, password);
    return true;
}

bool CloudSetupDialog::finishSyncPassword() {
    const QString password = m_syncPassword->text();
    if (m_newAccount) {
        if (password.length() < kMinSyncPasswordLength) {
            QMessageBox::warning(this, windowTitle(),
                QString("Choose a sync password of at least %1 characters.")
                    .arg(kMinSyncPasswordLength));
            return false;
        }
        if (password != m_syncConfirm->text()) {
            QMessageBox::warning(this, windowTitle(), "The two sync passwords don't match.");
            return false;
        }
    } else {
        if (password.isEmpty()) {
            QMessageBox::warning(this, windowTitle(), "Enter your sync password.");
            return false;
        }
        Sync::KeyCheck check = Sync::KeyCheck::NoCloudData;
        try {
            const FirebaseConfig config = CloudConfig::firebaseConfig();
            runBusy(this, "Checking your sync password…",
                    [&] { check = Sync::checkSyncPassword(config, password); });
        } catch (const std::exception &e) {
            QMessageBox::warning(this, windowTitle(), reason(e));
            return false;
        }
        if (check == Sync::KeyCheck::DoesNotMatch) {
            const auto answer = QMessageBox::warning(
                this, windowTitle(),
                "That sync password doesn't open your online copy.\n\n"
                "Check for typing mistakes and try again. If you have lost it, you can keep "
                "this new one: your next sync will offer to upload everything again under it.",
                QMessageBox::Retry | QMessageBox::Ignore, QMessageBox::Retry);
            if (answer != QMessageBox::Ignore) return false;
        }
    }
    CloudConfig::saveSyncPassword(password, m_remember->isChecked());
    return true;
}

void CloudSetupDialog::sendReset() {
    const QString email = m_email->text().trimmed();
    if (email.isEmpty()) {
        QMessageBox::information(this, windowTitle(), "Enter your email address first.");
        return;
    }
    try {
        const FirebaseConfig config = CloudConfig::firebaseConfig();
        runBusy(this, "Sending…", [&] { Sync::sendPasswordReset(config, email); });
        QMessageBox::information(this, windowTitle(),
            QString("If %1 has an account, a password reset email is on its way. Follow the "
                    "link in it, then sign in here with your new password.").arg(email));
    } catch (const std::exception &e) {
        QMessageBox::warning(this, windowTitle(), reason(e));
    }
}
