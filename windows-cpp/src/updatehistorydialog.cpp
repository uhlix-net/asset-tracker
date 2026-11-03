#include "updatehistorydialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

namespace {

struct Release {
    const char *version;
    const char *title;
    QStringList notes;
};

// Newest first.
const Release kHistory[] = {
    {"0.2.0", "Online Accounts and Faster Sync", {
        "Create your online sync account from inside the app. A short setup runs the first "
        "time Asset Tracker starts, or any time from File → Preferences",
        "Sign in to an existing account on a new PC, with a link to reset a forgotten password",
        "New File → Preferences: change your account password, have a reset link emailed to "
        "you, remember or change your sync password, sign out, and switch accounts",
        "Delete Online Data removes everything stored online but keeps your account. Delete "
        "Account removes the online data and the account. Both warn that this is permanent and "
        "ask you to type DELETE to confirm",
        "Sync uploads only what changed since your last sync, so it is much faster after the "
        "first time. Anything missing from your online copy is sent again automatically",
        "After a Restore from Cloud, the next sync has nothing to upload",
        "Sync Now (Ctrl+Shift+S) and Restore from Cloud are now directly in the File menu",
        "Clearer messages when signing in fails, such as a wrong password or an account that "
        "already exists",
        "Fixed a crash when syncing on PCs with an older Microsoft Visual C++ runtime. The "
        "installer now includes the runtime the app needs",
        "If Asset Tracker ever crashes, it now saves a report that helps us find the cause",
    }},
    {"0.1.0", "Initial C++ Release", {
        "Rewritten in C++ and Qt 6 for speed. Existing data, settings and cloud copies carry "
        "over unchanged",
        "A cleaner, more consistent look throughout",
        "Firebase and sync passwords are stored encrypted for your Windows account, and any "
        "saved in plain text are converted at first start",
        "Backups restore on any PC or Windows account",
        "A wrong backup password or damaged archive is caught before your data is touched, "
        "and restoring no longer needs a restart",
        "New: Restore from Cloud, for recovering everything on a new PC",
        "Sync removes deleted photos from the cloud, and asks before deleting assets that are "
        "no longer on this PC",
        "Sync stops if the sync password does not match the cloud copy",
        "Opening a receipt shows the receipt rather than the encrypted file",
        "Purchase price and current value columns sort by amount",
    }},
};

} // namespace

UpdateHistoryDialog::UpdateHistoryDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Asset Tracker — Update History");
    setMinimumSize(520, 420);
    resize(560, 500);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 12);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto *container = new QWidget;
    scroll->setWidget(container);
    auto *layout = new QVBoxLayout(container);
    layout->setAlignment(Qt::AlignTop);
    layout->setContentsMargins(24, 20, 24, 8);
    layout->setSpacing(6);

    for (const Release &release : kHistory) {
        auto *heading = new QLabel(QString("Version %1").arg(release.version));
        heading->setObjectName("version");
        auto *title = new QLabel(release.title);
        title->setObjectName("subtitle");
        layout->addWidget(heading);
        layout->addWidget(title);
        layout->addSpacing(6);

        for (const QString &note : release.notes) {
            auto *row = new QLabel(QStringLiteral("•  ") + note);
            row->setWordWrap(true);
            row->setTextFormat(Qt::PlainText);
            layout->addWidget(row);
        }
        layout->addSpacing(14);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->setContentsMargins(24, 0, 24, 0);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    outer->addWidget(buttons);
}
