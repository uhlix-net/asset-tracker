from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QFormLayout, QLineEdit, QDateEdit,
    QDialogButtonBox, QLabel,
)
from PyQt6.QtCore import Qt, QDate, QSettings
from ..config import APP_NAME

_SETTINGS_KEYS = {
    "company":       "insurer/company",
    "policy_number": "insurer/policy_number",
    "claim_number":  "insurer/claim_number",
    "date_of_loss":  "insurer/date_of_loss",
}


def load_insurer_info() -> dict:
    s = QSettings(APP_NAME, APP_NAME)
    return {k: s.value(_SETTINGS_KEYS[k], "") for k in _SETTINGS_KEYS}


def save_insurer_info(info: dict) -> None:
    s = QSettings(APP_NAME, APP_NAME)
    for k, v in info.items():
        if k in _SETTINGS_KEYS:
            s.setValue(_SETTINGS_KEYS[k], v)


class InsurerInfoDialog(QDialog):
    """Collects insurance company details before generating a report."""

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle("Insurance Claim Information")
        self.setMinimumWidth(420)

        layout = QVBoxLayout(self)

        note = QLabel(
            "This information will appear on the report cover page.\n"
            "All fields are optional — leave blank if unknown."
        )
        note.setStyleSheet("color: #555; font-size: 10px;")
        note.setWordWrap(True)
        layout.addWidget(note)

        form = QFormLayout()
        form.setLabelAlignment(Qt.AlignmentFlag.AlignRight)

        saved = load_insurer_info()

        self._company = QLineEdit(saved["company"])
        self._company.setPlaceholderText("e.g. State Farm Insurance")
        form.addRow("Insurance Company:", self._company)

        self._policy = QLineEdit(saved["policy_number"])
        self._policy.setPlaceholderText("e.g. PO-1234567")
        form.addRow("Policy Number:", self._policy)

        self._claim = QLineEdit(saved["claim_number"])
        self._claim.setPlaceholderText("e.g. CLM-2026-00001")
        form.addRow("Claim Number:", self._claim)

        self._loss_date = QLineEdit(saved["date_of_loss"])
        self._loss_date.setPlaceholderText("e.g. May 1, 2026")
        form.addRow("Date of Loss:", self._loss_date)

        layout.addLayout(form)

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok |
            QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self._on_accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def _on_accept(self) -> None:
        info = self.insurer_info()
        save_insurer_info(info)
        self.accept()

    def insurer_info(self) -> dict:
        return {
            "company":       self._company.text().strip(),
            "policy_number": self._policy.text().strip(),
            "claim_number":  self._claim.text().strip(),
            "date_of_loss":  self._loss_date.text().strip(),
        }
