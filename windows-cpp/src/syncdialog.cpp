#include "syncdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "backup.h"
#include "cloudconfig.h"
#include "database.h"

SyncDialog::SyncDialog(Database *db, Mode mode, QWidget *parent)
    : QDialog(parent)
    , m_db(db)
    , m_mode(mode)
{
    setWindowTitle(mode == Mode::Restore ? "Restore from Cloud" : "Online Sync");
    setMinimumWidth(440);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    m_heading = new QLabel(mode == Mode::Restore ? "Restoring from the cloud"
                           : mode == Mode::Rekey ? "Uploading under the new sync password"
                                                 : "Syncing your inventory");
    m_heading->setObjectName("title");
    m_status = new QLabel("Starting…");
    m_status->setWordWrap(true);
    m_progress = new QProgressBar;
    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    m_close = buttons->button(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &SyncDialog::reject);

    layout->addWidget(m_heading);
    layout->addWidget(m_status);
    layout->addWidget(m_progress);
    layout->addStretch();
    layout->addWidget(buttons);
}

void SyncDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    if (m_started) return;
    m_started = true;
    QTimer::singleShot(0, this, &SyncDialog::start);
}

void SyncDialog::start() {
    m_config = CloudConfig::firebaseConfig();
    m_syncPassword = CloudConfig::syncPassword();
    if (m_syncPassword.isEmpty()) {
        bool ok = false;
        m_syncPassword = QInputDialog::getText(this, "Sync Password", "Enter your sync password:",
                                               QLineEdit::Password, QString(), &ok);
        if (!ok || m_syncPassword.isEmpty()) {
            QDialog::reject();
            return;
        }
    }

    if (m_mode == Mode::Restore) startRestore();
    else startPush(m_mode == Mode::Rekey);
}

void SyncDialog::setStatus(const QString &text) {
    // A trail in debug.log, so a sync that dies shows how far it got. Never secrets.
    qInfo("Sync: %s", qPrintable(text));
    // Worker threads report through this; the call is queued onto the UI thread.
    QMetaObject::invokeMethod(m_status, "setText", Qt::QueuedConnection, Q_ARG(QString, text));
}

void SyncDialog::startWork(const std::function<void()> &work, const std::function<void()> &done) {
    m_close->setEnabled(false);
    m_progress->show();
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
        m_close->setEnabled(true);
        m_progress->hide();
        if (m_error.isEmpty()) done();
        else finishWithError(m_error);
    });
    m_thread->start();
}

void SyncDialog::finishWithError(const QString &message) {
    qWarning("Sync failed: %s", qPrintable(message));
    m_heading->setText("Something went wrong");
    m_status->setText(message);
    m_progress->hide();
    QMessageBox::critical(this, windowTitle(), message);
}

void SyncDialog::reject() {
    if (!m_thread) QDialog::reject();
}

void SyncDialog::startPush(bool allowKeyChange) {
    // SQLite connections stay on their own thread, so the worker gets a snapshot.
    QVector<AssetRecord> records;
    SyncState state;
    try {
        records = m_db->getAllAssetsWithFiles();
        state = m_db->loadSyncState();
    } catch (const std::exception &e) {
        finishWithError(QString::fromUtf8(e.what()));
        return;
    }

    startWork([this, records, state, allowKeyChange] {
        m_pushResult = Sync::push(m_config, m_syncPassword, records, state, allowKeyChange,
                                  [this](const QString &text) { setStatus(text); });
    }, [this] { finishPush(); });
}

void SyncDialog::finishPush() {
    const Sync::PushResult &result = m_pushResult;
    if (result.keyMismatch) {
        m_heading->setText("Sync password doesn't match");
        m_status->setText("Nothing was uploaded.");
        const auto answer = QMessageBox::warning(
            this, "Different Sync Password",
            "This sync password does not match the data already in your online copy.\n\n"
            "If you mistyped it, choose No and try again. Choose Yes only if you have lost "
            "the old one: everything on this PC is uploaded again under this password, and "
            "the Android app will need it too.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) startPush(true);
        return;
    }

    try {
        m_db->saveSyncState(result.state);
    } catch (const std::exception &e) {
        finishWithError(QString("Synced, but could not record it: %1").arg(e.what()));
        return;
    }

    m_heading->setText("Sync complete");
    QString message;
    if (result.assets + result.files == 0) {
        message = QString("Everything was already up to date (%1 items, %2 files).")
                      .arg(result.unchangedAssets).arg(result.unchangedFiles);
    } else {
        message = QString("Uploaded %1 changed item%2 and %3 file%4. %5 item%6 and %7 file%8 "
                          "were already up to date.")
                      .arg(result.assets).arg(result.assets == 1 ? "" : "s")
                      .arg(result.files).arg(result.files == 1 ? "" : "s")
                      .arg(result.unchangedAssets).arg(result.unchangedAssets == 1 ? "" : "s")
                      .arg(result.unchangedFiles).arg(result.unchangedFiles == 1 ? "" : "s");
    }
    if (!result.errors.isEmpty()) {
        message += QString("\n\nProblems (%1): %2").arg(result.errors.size())
                       .arg(result.errors.mid(0, 3).join("; "));
    }
    m_status->setText(message);

    if (!result.cloudOnly.isEmpty()) offerCloudDeletion();
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
        m_status->setText(QString("%1\n\nDeleted from the cloud: %2").arg(summary).arg(ids.size()));
    });
}

void SyncDialog::startRestore() {
    const auto answer = QMessageBox::warning(
        this, "Restore from Cloud",
        "This replaces all data on this PC with your online copy.\n"
        "Your existing data is moved to a timestamped folder first.\n\nContinue?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        QDialog::reject();
        return;
    }

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
            m_heading->setText("Restore cancelled");
            m_status->setText("Nothing was changed.");
            return;
        }
    }

    try {
        const QString aside = Backup::commitRestore(result.stagingDir, *m_db);
        m_heading->setText("Restore complete");
        QString message = QString("Restored %1 items and %2 files.").arg(result.assets)
                              .arg(result.files);
        if (!aside.isEmpty())
            message += QString("\n\nYour previous data was kept in:\n%1").arg(aside);
        m_status->setText(message);
        emit dataReplaced();
    } catch (const std::exception &e) {
        QDir(result.stagingDir).removeRecursively();
        finishWithError(QString::fromUtf8(e.what()));
    }
}
