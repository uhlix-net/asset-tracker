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
