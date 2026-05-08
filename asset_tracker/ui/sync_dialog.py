from __future__ import annotations

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QFormLayout, QLineEdit, QPushButton,
    QDialogButtonBox, QLabel, QProgressBar, QCheckBox, QMessageBox,
    QTabWidget, QWidget,
)
from PyQt6.QtCore import Qt, QSettings, QThread, pyqtSignal

from ..config import APP_NAME

_KEYS = {
    "api_key":       "firebase/api_key",
    "project_id":    "firebase/project_id",
    "bucket":        "firebase/bucket",
    "email":         "firebase/email",
    "password":      "firebase/password",
    "sync_password": "firebase/sync_password",
    "remember_sync": "firebase/remember_sync_password",
}


def _load(s: QSettings) -> dict:
    return {k: s.value(_KEYS[k], "") for k in _KEYS}


def _save(s: QSettings, values: dict) -> None:
    for k, v in values.items():
        if k in _KEYS:
            s.setValue(_KEYS[k], v)


# ── Background sync worker ────────────────────────────────────────────────────

class _SyncWorker(QThread):
    progress = pyqtSignal(str)
    finished = pyqtSignal(dict)
    error    = pyqtSignal(str)

    def __init__(self, db, cfg: dict, sync_key: bytes) -> None:
        super().__init__()
        self._db       = db
        self._cfg      = cfg
        self._sync_key = sync_key

    def run(self) -> None:
        try:
            from ..sync import FirebaseSync
            fs = FirebaseSync(self._cfg["api_key"],
                              self._cfg["project_id"],
                              self._cfg["bucket"])
            self.progress.emit("Authenticating with Firebase…")
            fs.authenticate(self._cfg["email"], self._cfg["password"])
            result = fs.push_all(self._db, self._sync_key,
                                 progress=self.progress.emit)
            self.finished.emit(result)
        except Exception as exc:
            self.error.emit(str(exc))


# ── Main dialog ───────────────────────────────────────────────────────────────

class SyncDialog(QDialog):
    def __init__(self, db, parent=None) -> None:
        super().__init__(parent)
        self._db       = db
        self._settings = QSettings(APP_NAME, APP_NAME)
        self._worker: _SyncWorker | None = None

        self.setWindowTitle("Cloud Sync — Firebase")
        self.setMinimumWidth(460)
        self._build_ui()
        self._load_settings()

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)

        tabs = QTabWidget()
        layout.addWidget(tabs)

        # ── Firebase config tab ───────────────────────────────────────────────
        cfg_tab = QWidget()
        cfg_form = QFormLayout(cfg_tab)
        cfg_form.setLabelAlignment(Qt.AlignmentFlag.AlignRight)

        self._api_key    = QLineEdit()
        self._project_id = QLineEdit()
        self._bucket     = QLineEdit()
        self._bucket.setPlaceholderText("your-project-id.appspot.com")
        self._email      = QLineEdit()
        self._email.setPlaceholderText("user@example.com")
        self._fb_pass    = QLineEdit()
        self._fb_pass.setEchoMode(QLineEdit.EchoMode.Password)

        cfg_form.addRow("Firebase API Key:", self._api_key)
        cfg_form.addRow("Project ID:", self._project_id)
        cfg_form.addRow("Storage Bucket:", self._bucket)
        cfg_form.addRow("Firebase Email:", self._email)
        cfg_form.addRow("Firebase Password:", self._fb_pass)

        note = QLabel(
            "Enter the Firebase credentials for your project.\n"
            "See  Help → AssetTracker Help  for setup instructions."
        )
        note.setStyleSheet("color: #555; font-size: 10px;")
        note.setWordWrap(True)
        cfg_form.addRow(note)
        tabs.addTab(cfg_tab, "Firebase Config")

        # ── Sync password tab ─────────────────────────────────────────────────
        sp_tab  = QWidget()
        sp_form = QFormLayout(sp_tab)
        sp_form.setLabelAlignment(Qt.AlignmentFlag.AlignRight)

        self._sync_pass = QLineEdit()
        self._sync_pass.setEchoMode(QLineEdit.EchoMode.Password)
        self._remember  = QCheckBox("Remember sync password (stored in Windows registry)")

        enc_note = QLabel(
            "🔒  All data is AES-256-GCM encrypted with this password "
            "before being sent to Firebase.\n"
            "The same password must be entered in the Android app.\n"
            "Google cannot read your files."
        )
        enc_note.setStyleSheet("color: #1B2A3B; font-size: 10px;")
        enc_note.setWordWrap(True)

        sp_form.addRow("Sync Password:", self._sync_pass)
        sp_form.addRow(self._remember)
        sp_form.addRow(enc_note)
        tabs.addTab(sp_tab, "Encryption")

        # ── Progress / status ─────────────────────────────────────────────────
        self._status = QLabel("Ready.")
        self._status.setWordWrap(True)
        self._progress = QProgressBar()
        self._progress.setRange(0, 0)   # indeterminate
        self._progress.setVisible(False)
        layout.addWidget(self._status)
        layout.addWidget(self._progress)

        # ── Buttons ───────────────────────────────────────────────────────────
        self._btn_sync = QPushButton("Sync Now")
        self._btn_sync.clicked.connect(self._on_sync)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Save |
                                   QDialogButtonBox.StandardButton.Close)
        buttons.button(QDialogButtonBox.StandardButton.Save).setText("Save Settings")
        buttons.accepted.connect(self._on_save)
        buttons.rejected.connect(self.reject)

        layout.addWidget(self._btn_sync)
        layout.addWidget(buttons)

    def _load_settings(self) -> None:
        v = _load(self._settings)
        self._api_key.setText(v["api_key"])
        self._project_id.setText(v["project_id"])
        self._bucket.setText(v["bucket"])
        self._email.setText(v["email"])
        self._fb_pass.setText(v["password"])
        remember = str(v["remember_sync"]).lower() == "true"
        self._remember.setChecked(remember)
        if remember:
            self._sync_pass.setText(v["sync_password"])

    def _on_save(self) -> None:
        values = {
            "api_key":       self._api_key.text().strip(),
            "project_id":    self._project_id.text().strip(),
            "bucket":        self._bucket.text().strip(),
            "email":         self._email.text().strip(),
            "password":      self._fb_pass.text(),
            "remember_sync": str(self._remember.isChecked()),
            "sync_password": self._sync_pass.text() if self._remember.isChecked() else "",
        }
        _save(self._settings, values)
        QMessageBox.information(self, "Saved", "Cloud sync settings saved.")

    def _on_sync(self) -> None:
        cfg = {
            "api_key":    self._api_key.text().strip(),
            "project_id": self._project_id.text().strip(),
            "bucket":     self._bucket.text().strip(),
            "email":      self._email.text().strip(),
            "password":   self._fb_pass.text(),
        }
        sync_password = self._sync_pass.text()

        missing = [k for k, v in cfg.items() if not v]
        if missing or not sync_password:
            QMessageBox.warning(self, "Incomplete",
                                "Please fill in all Firebase fields and the sync password.")
            return

        from ..sync import derive_key
        sync_key = derive_key(sync_password)

        self._btn_sync.setEnabled(False)
        self._progress.setVisible(True)
        self._status.setText("Connecting…")

        self._worker = _SyncWorker(self._db, cfg, sync_key)
        self._worker.progress.connect(self._status.setText)
        self._worker.finished.connect(self._on_finished)
        self._worker.error.connect(self._on_error)
        self._worker.start()

    def _on_finished(self, result: dict) -> None:
        self._progress.setVisible(False)
        self._btn_sync.setEnabled(True)
        errs = result.get("errors", [])
        msg = (f"Sync complete.\n"
               f"Assets: {result['assets']}  |  Files: {result['files']}")
        if errs:
            msg += f"\nErrors ({len(errs)}): {'; '.join(errs[:3])}"
        self._status.setText(msg)

    def _on_error(self, msg: str) -> None:
        self._progress.setVisible(False)
        self._btn_sync.setEnabled(True)
        self._status.setText(f"Error: {msg}")
        QMessageBox.critical(self, "Sync Failed", msg)
