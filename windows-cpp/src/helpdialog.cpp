#include "helpdialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include "config.h"

namespace {

struct Entry {
    const char *heading;
    const char *body;
};

const Entry kHelp[] = {
    {"PHOTO AND RECEIPT ENCRYPTION", nullptr},
    {nullptr, "Asset Tracker automatically encrypts every photo and receipt file you upload "
              "using <b>Windows Data Protection API (DPAPI)</b> before writing it to disk."},

    {"What is Windows DPAPI?", nullptr},
    {nullptr, "DPAPI is a built-in Windows encryption feature that ties your encrypted data "
              "to your Windows user account. This means:"},
    {nullptr, "• Your files cannot be read by other Windows user accounts on the same PC."},
    {nullptr, "• Your files are protected if someone removes or copies your hard drive."},
    {nullptr, "• Decryption is automatic — no password is required when using the app."},
    {nullptr, "• Encryption keys are managed entirely by Windows, not by Asset Tracker."},

    {"Where are encrypted files stored?", nullptr},
    {nullptr, "Encrypted files are stored in:<br>"
              "<tt>%APPDATA%\\AssetTracker\\assets\\</tt><br><br>"
              "Each file keeps its original extension (.jpg, .pdf, etc.) but its contents are "
              "encrypted binary data. Image viewers, PDF readers, and other programs cannot "
              "open these files directly."},

    {"Obtaining unencrypted copies", nullptr},
    {nullptr, "If you need unencrypted copies of your files — for example, to attach photos "
              "directly to an insurance claim email — use:"},
    {nullptr, "<b>File → Export Asset Files…</b>"},
    {nullptr, "This decrypts all files and copies them to a folder you choose. The originals "
              "stored by Asset Tracker are not affected."},

    {"Encrypted backups", nullptr},
    {nullptr, "Backups created via <b>File → Create Backup…</b> are AES-256 encrypted ZIP "
              "archives protected by a password you choose. Files inside are protected by that "
              "password alone, not by DPAPI, so a backup restores on any PC. Keep the password "
              "safe: it cannot be recovered."},

    {"Transferring data to another computer", nullptr},
    {nullptr, "1. On the old PC: <b>File → Create Backup…</b> (choose a strong password)."},
    {nullptr, "2. Copy the backup ZIP to the new PC."},
    {nullptr, "3. On the new PC: <b>File → Restore from Backup…</b> and enter the password."},
    {nullptr, "Restored files are encrypted for your account on the new PC automatically."},

    {"Cloud sync and restore", nullptr},
    {nullptr, "<b>File → Sync to Cloud (Firebase)…</b> encrypts your assets with your sync "
              "password before they leave this PC. Google stores only ciphertext. Your Firebase "
              "and sync passwords are stored encrypted for your Windows account."},
    {nullptr, "If you have lost this PC, install Asset Tracker on a new one, enter the same "
              "Firebase details and sync password, and choose <b>Restore from Cloud…</b>. "
              "If the sync password is lost, the cloud copy cannot be recovered."},
    {nullptr, "When the cloud holds assets this PC does not, Sync asks before deleting them. "
              "Answer No if this PC is missing data that you want to keep."},

    {"Security reminder", nullptr},
    {nullptr, "Unencrypted files exported via <b>File → Export Asset Files…</b> should be "
              "treated as sensitive. Store or transmit them securely (e.g. over an encrypted "
              "email connection or HTTPS file upload)."},
};

} // namespace

HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Asset Tracker Help");
    setMinimumSize(560, 500);
    resize(600, 580);

    auto *outer = new QVBoxLayout(this);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto *container = new QWidget;
    scroll->setWidget(container);
    auto *layout = new QVBoxLayout(container);
    layout->setAlignment(Qt::AlignTop);
    layout->setContentsMargins(20, 8, 20, 16);
    layout->setSpacing(6);

    for (const Entry &entry : kHelp) {
        if (entry.heading) {
            auto *label = new QLabel(QString(entry.heading).toUpper());
            label->setObjectName("section");
            layout->addWidget(label);
        }
        if (entry.body) {
            auto *label = new QLabel(entry.body);
            label->setWordWrap(true);
            label->setTextFormat(Qt::RichText);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            layout->addWidget(label);
        }
    }
    layout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    outer->addWidget(buttons);
}
