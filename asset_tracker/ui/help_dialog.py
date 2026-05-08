from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QLabel, QDialogButtonBox,
    QScrollArea, QWidget, QFrame,
)
from PyQt6.QtCore import Qt
from ..config import APP_NAME, APP_VERSION

_HELP_TEXT = [
    ("PHOTO AND RECEIPT ENCRYPTION", None),

    (None, (
        f"{APP_NAME} automatically encrypts every photo and receipt file "
        "you upload using <b>Windows Data Protection API (DPAPI)</b> before "
        "writing it to disk."
    )),

    ("What is Windows DPAPI?", None),
    (None, (
        "DPAPI is a built-in Windows encryption feature that ties your "
        "encrypted data to your Windows user account. This means:"
    )),
    (None, "• Your files cannot be read by other Windows user accounts on the same PC."),
    (None, "• Your files are protected if someone removes or copies your hard drive."),
    (None, "• Decryption is automatic — no password is required when using the app."),
    (None, "• Encryption keys are managed entirely by Windows, not by Asset Tracker."),

    ("Where are encrypted files stored?", None),
    (None, (
        "Encrypted files are stored in:<br>"
        "<tt>%APPDATA%\\AssetTracker\\assets\\</tt><br><br>"
        "Each file keeps its original extension (.jpg, .pdf, etc.) but its "
        "contents are encrypted binary data. Image viewers, PDF readers, and "
        "other programs cannot open these files directly."
    )),

    ("Obtaining unencrypted copies", None),
    (None, (
        "If you need unencrypted copies of your files — for example, to attach "
        "photos directly to an insurance claim email — use:"
    )),
    (None, "<b>File → Export Asset Files…</b>"),
    (None, (
        "This decrypts all files and copies them to a folder you choose. "
        "The originals stored by Asset Tracker are not affected."
    )),

    ("Encrypted backups", None),
    (None, (
        "Backups created via <b>File → Create Backup…</b> are AES-256 encrypted "
        "ZIP archives protected by a password you choose. The backup encryption "
        "is independent of DPAPI, making backups safe to store off-site or "
        "on removable media."
    )),

    ("Transferring data to another computer", None),
    (None, (
        "DPAPI encryption is tied to your Windows user account and machine. "
        "To move Asset Tracker data to another PC:"
    )),
    (None, "1. On the old PC: <b>File → Create Backup…</b> (choose a strong password)."),
    (None, "2. Copy the backup ZIP to the new PC."),
    (None, "3. On the new PC: <b>File → Restore from Backup…</b> and enter the password."),
    (None, (
        "After restore, Asset Tracker re-encrypts all files with the new PC's "
        "DPAPI key automatically on the next launch."
    )),

    ("Security reminder", None),
    (None, (
        "Unencrypted files exported via <b>File → Export Asset Files…</b> "
        "should be treated as sensitive. Store or transmit them securely "
        "(e.g. over an encrypted email connection or HTTPS file upload)."
    )),
]


class HelpDialog(QDialog):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle(f"{APP_NAME} Help")
        self.setMinimumWidth(560)
        self.setMinimumHeight(500)
        self.resize(600, 580)

        outer = QVBoxLayout(self)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        outer.addWidget(scroll)

        container = QWidget()
        scroll.setWidget(container)
        layout = QVBoxLayout(container)
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        layout.setSpacing(6)

        for heading, body in _HELP_TEXT:
            if heading is not None:
                lbl = QLabel(heading)
                lbl.setStyleSheet(
                    "font-size: 12px; font-weight: bold; "
                    "color: #1B2A3B; margin-top: 10px;"
                )
                layout.addWidget(lbl)
                sep = QFrame()
                sep.setFrameShape(QFrame.Shape.HLine)
                sep.setStyleSheet("color: #c0c8d0;")
                layout.addWidget(sep)
            if body is not None:
                lbl = QLabel(body)
                lbl.setWordWrap(True)
                lbl.setTextFormat(Qt.TextFormat.RichText)
                lbl.setStyleSheet("color: #333; font-size: 10px; padding-left: 4px;")
                layout.addWidget(lbl)

        layout.addStretch()

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Close)
        buttons.rejected.connect(self.accept)
        outer.addWidget(buttons)
