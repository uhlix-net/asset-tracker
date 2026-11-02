#include "syncdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>

#include "backup.h"
#include "config.h"
#include "database.h"
#include "secretstore.h"

namespace {

const char kPasswordKey[] = "firebase/password";
const char kSyncPasswordKey[] = "firebase/sync_password";
const char kEncSuffix[] = "_enc";

// Reads a password, stored only in its DPAPI form.
QString readSecret(QSettings &settings, const QString &key) {
    return SecretStore::unprotect(settings.value(key + kEncSuffix).toString());
}

void writeSecret(QSettings &settings, const QString &key, const QString &value) {
    settings.setValue(key + kEncSuffix, SecretStore::protect(value));
    settings.remove(key);
}

void migrateSecret(QSettings &settings, const QString &key) {
    const QString plain = settings.value(key).toString();
    if (!settings.contains(key)) return;
    // If DPAPI failed here, leave the value for the next start rather than lose it.
    const QString sealed = SecretStore::protect(plain);
    if (plain.isEmpty() || !sealed.isEmpty()) {
        if (!plain.isEmpty()) settings.setValue(key + kEncSuffix, sealed);
        settings.remove(key);
    }
}

} // namespace

void migrateStoredPasswords() {
    const auto settings = Config::openSettings();
    migrateSecret(*settings, kPasswordKey);
    migrateSecret(*settings, kSyncPasswordKey);
}

SyncDialog::SyncDialog(Database *db, QWidget *parent)
    : QDialog(parent)
    , m_db(db)
{
    setWindowTitle("Cloud Sync — Firebase");
    setMinimumWidth(460);
    buildUi();
    loadSettings();
}

void SyncDialog::buildUi() {
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget;
    layout->addWidget(tabs);

    auto *configTab = new QWidget;
    auto *configForm = new QFormLayout(configTab);
    configForm->setLabelAlignment(Qt::AlignRight);

    m_apiKey = new QLineEdit;
    m_projectId = new QLineEdit;
    m_bucket = new QLineEdit;
    m_bucket->setPlaceholderText("your-project-id.appspot.com");
    m_email = new QLineEdit;
    m_email->setPlaceholderText("user@example.com");
    m_firebasePassword = new QLineEdit;
    m_firebasePassword->setEchoMode(QLineEdit::Password);

    configForm->addRow("Firebase API Key:", m_apiKey);
    configForm->addRow("Project ID:", m_projectId);
    configForm->addRow("Storage Bucket:", m_bucket);
    configForm->addRow("Firebase Email:", m_email);
    configForm->addRow("Firebase Password:", m_firebasePassword);

    auto *note = new QLabel("Enter the Firebase credentials for your project.\n"
                            "Passwords are stored encrypted for your Windows account.");
    note->setStyleSheet("color: #555; font-size: 10px;");
    note->setWordWrap(true);
    configForm->addRow(note);
    tabs->addTab(configTab, "Firebase Config");

    auto *passwordTab = new QWidget;
    auto *passwordForm = new QFormLayout(passwordTab);
    passwordForm->setLabelAlignment(Qt::AlignRight);

    m_syncPasswordEdit = new QLineEdit;
    m_syncPasswordEdit->setEchoMode(QLineEdit::Password);
    m_remember = new QCheckBox("Remember sync password (encrypted for this Windows account)");

    auto *encNote = new QLabel("🔒  All data is AES-256-GCM encrypted with this password "
                               "before being sent to Firebase.\n"
                               "The same password must be entered in the Android app.\n"
                               "Google cannot read your files.");
    encNote->setStyleSheet("color: #1B2A3B; font-size: 10px;");
    encNote->setWordWrap(true);

    passwordForm->addRow("Sync Password:", m_syncPasswordEdit);
    passwordForm->addRow(m_remember);
    passwordForm->addRow(encNote);
    tabs->addTab(passwordTab, "Encryption");

    m_status = new QLabel("Ready.");
    m_status->setWordWrap(true);
    m_progress = new QProgressBar;
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    layout->addWidget(m_status);
    layout->addWidget(m_progress);

    m_syncButton = new QPushButton("Sync Now");
    connect(m_syncButton, &QPushButton::clicked, this, &SyncDialog::onSync);
    m_restoreButton = new QPushButton("Restore from Cloud...");
    connect(m_restoreButton, &QPushButton::clicked, this, &SyncDialog::onRestore);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close);
    buttons->button(QDialogButtonBox::Save)->setText("Save Settings");
    connect(buttons, &QDialogButtonBox::accepted, this, &SyncDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &SyncDialog::reject);

    layout->addWidget(m_syncButton);
    layout->addWidget(m_restoreButton);
    layout->addWidget(buttons);
}

void SyncDialog::loadSettings() {
    const auto settings = Config::openSettings();
    m_apiKey->setText(settings->value("firebase/api_key").toString());
    m_projectId->setText(settings->value("firebase/project_id").toString());
    m_bucket->setText(settings->value("firebase/bucket").toString());
    m_email->setText(settings->value("firebase/email").toString());
    m_firebasePassword->setText(readSecret(*settings, kPasswordKey));

    // Python stored this as the string "True"; QVariant reads that as true too.
    const bool remember = settings->value("firebase/remember_sync_password").toBool();
    m_remember->setChecked(remember);
    if (remember) m_syncPasswordEdit->setText(readSecret(*settings, kSyncPasswordKey));
}

void SyncDialog::onSave() {
    const auto settings = Config::openSettings();
    settings->setValue("firebase/api_key", m_apiKey->text().trimmed());
    settings->setValue("firebase/project_id", m_projectId->text().trimmed());
    settings->setValue("firebase/bucket", m_bucket->text().trimmed());
    settings->setValue("firebase/email", m_email->text().trimmed());
    settings->setValue("firebase/remember_sync_password", m_remember->isChecked());
    writeSecret(*settings, kPasswordKey, m_firebasePassword->text());
    writeSecret(*settings, kSyncPasswordKey,
                m_remember->isChecked() ? m_syncPasswordEdit->text() : QString());
    QMessageBox::information(this, "Saved", "Cloud sync settings saved.");
}

bool SyncDialog::readForm(FirebaseConfig *config, QString *syncPassword) {
    config->apiKey = m_apiKey->text().trimmed();
    config->projectId = m_projectId->text().trimmed();
    config->bucket = m_bucket->text().trimmed();
    config->email = m_email->text().trimmed();
    config->password = m_firebasePassword->text();
    *syncPassword = m_syncPasswordEdit->text();

    if (config->apiKey.isEmpty() || config->projectId.isEmpty() || config->bucket.isEmpty()
        || config->email.isEmpty() || config->password.isEmpty() || syncPassword->isEmpty()) {
        QMessageBox::warning(this, "Incomplete",
                             "Please fill in all Firebase fields and the sync password.");
        return false;
    }
    return true;
}

void SyncDialog::setBusy(bool busy) {
    m_syncButton->setEnabled(!busy);
    m_restoreButton->setEnabled(!busy);
    m_progress->setVisible(busy);
}

// Worker threads report through this; the call is queued onto the UI thread.
void SyncDialog::setStatus(const QString &text) {
    QMetaObject::invokeMethod(m_status, "setText", Qt::QueuedConnection, Q_ARG(QString, text));
}

void SyncDialog::startWork(const std::function<void()> &work, const std::function<void()> &done) {
    setBusy(true);
    m_error.clear();
    m_thread = QThread::create([this, work] {
        try {
            work();
        } catch (const std::exception &e) {
            m_error = QString::fromUtf8(e.what());
        }
    });
    connect(m_thread, &QThread::finished, this, [this, done] {
        m_thread->deleteLater();
        m_thread = nullptr;
        setBusy(false);
        if (m_error.isEmpty()) done();
        else showFailure();
    });
    m_thread->start();
}

void SyncDialog::showFailure() {
    m_status->setText("Error: " + m_error);
    QMessageBox::critical(this, "Cloud Sync Failed", m_error);
}

void SyncDialog::reject() {
    if (!m_thread) QDialog::reject();
}

void SyncDialog::onSync() {
    if (readForm(&m_config, &m_syncPassword)) startPush(false);
}

void SyncDialog::startPush(bool allowKeyChange) {
    // SQLite connections stay on their own thread, so the worker gets a snapshot.
    QVector<AssetRecord> records;
    try {
        records = m_db->getAllAssetsWithFiles();
    } catch (const std::exception &e) {
        m_error = QString::fromUtf8(e.what());
        showFailure();
        return;
    }

    m_status->setText("Connecting…");
    startWork([this, records, allowKeyChange] {
        m_pushResult = Sync::push(m_config, m_syncPassword, records, allowKeyChange,
                                  [this](const QString &text) { setStatus(text); });
    }, [this] { finishPush(); });
}

void SyncDialog::finishPush() {
    if (m_pushResult.keyMismatch) {
        m_status->setText("Ready.");
        const auto answer = QMessageBox::warning(
            this, "Different Sync Password",
            "The sync password does not match the data already in the cloud.\n\n"
            "If you mistyped it, choose No. If you are changing the password, choose Yes: "
            "everything on this PC is uploaded again under the new password, and the Android "
            "app will need the new one.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) startPush(true);
        return;
    }

    QString message = QString("Sync complete.\nAssets: %1  |  Files: %2")
                          .arg(m_pushResult.assets).arg(m_pushResult.files);
    if (!m_pushResult.errors.isEmpty()) {
        message += QString("\nErrors (%1): %2").arg(m_pushResult.errors.size())
                       .arg(m_pushResult.errors.mid(0, 3).join("; "));
    }
    m_status->setText(message);

    if (!m_pushResult.cloudOnly.isEmpty()) offerCloudDeletion();
}

// Asking here, rather than deleting whatever is missing locally, is what keeps a
// damaged or partly restored PC from wiping the only good copy.
void SyncDialog::offerCloudDeletion() {
    QStringList names, ids;
    for (const Sync::CloudAsset &asset : m_pushResult.cloudOnly) {
        ids << asset.id;
        if (names.size() < 10)
            names << QString("  • %1 (%2)").arg(asset.name.isEmpty() ? "unreadable" : asset.name,
                                                asset.id);
    }
    if (ids.size() > 10) names << QString("  … and %1 more").arg(ids.size() - 10);

    const auto answer = QMessageBox::question(
        this, "Assets Only in the Cloud",
        QString("%1 asset(s) in the cloud are not on this PC:\n\n%2\n\n"
                "If you deleted them here, delete them from the cloud too? "
                "They will also disappear from the Android app. This cannot be undone.")
            .arg(ids.size()).arg(names.join('\n')),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    const QString summary = m_status->text();
    startWork([this, ids] {
        Sync::deleteFromCloud(m_config, m_syncPassword, ids,
                              [this](const QString &text) { setStatus(text); });
    }, [this, summary, ids] {
        m_status->setText(QString("%1\nDeleted from the cloud: %2").arg(summary).arg(ids.size()));
    });
}

void SyncDialog::onRestore() {
    if (!readForm(&m_config, &m_syncPassword)) return;
    const auto answer = QMessageBox::warning(
        this, "Restore from Cloud",
        "This replaces all data on this PC with the cloud copy.\n"
        "Your existing data is moved to a timestamped folder first.\n\nContinue?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    m_status->setText("Connecting…");
    startWork([this] {
        m_restoreResult = Sync::downloadToStaging(m_config, m_syncPassword,
            [this](const QString &text) { setStatus(text); });
    }, [this] { finishRestore(); });
}

void SyncDialog::finishRestore() {
    const Sync::RestoreResult &result = m_restoreResult;
    if (!result.errors.isEmpty()) {
        const auto answer = QMessageBox::warning(
            this, "Restore Incomplete",
            QString("%1 item(s) could not be downloaded:\n\n%2\n\n"
                    "Replace the data on this PC anyway?")
                .arg(result.errors.size()).arg(result.errors.mid(0, 5).join('\n')),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            QDir(result.stagingDir).removeRecursively();
            m_status->setText("Restore cancelled. Nothing was changed.");
            return;
        }
    }

    try {
        const QString aside = Backup::commitRestore(result.stagingDir, *m_db);
        m_status->setText(QString("Restored %1 assets and %2 files.").arg(result.assets)
                              .arg(result.files));
        emit dataReplaced();
        if (!aside.isEmpty()) {
            QMessageBox::information(this, "Restore Complete",
                                     QString("Your previous data was kept in:\n%1").arg(aside));
        }
    } catch (const std::exception &e) {
        QDir(result.stagingDir).removeRecursively();
        m_error = QString::fromUtf8(e.what());
        showFailure();
    }
}
