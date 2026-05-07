from __future__ import annotations
import pathlib
from datetime import datetime, timezone

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QFormLayout, QLineEdit, QDateEdit, QDoubleSpinBox,
    QCheckBox, QPushButton, QListWidget, QListWidgetItem, QDialogButtonBox,
    QLabel, QHBoxLayout, QFileDialog, QWidget, QMessageBox,
)
from PyQt6.QtCore import Qt, QDate

from ..database import Database
from ..models import Asset
from ..config import IMAGE_EXTENSIONS, RECEIPT_EXTENSIONS
from .. import storage


def _now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


class AssetFormDialog(QDialog):
    def __init__(self, db: Database, parent=None) -> None:
        super().__init__(parent)
        self._db = db
        self._photo_paths: list[pathlib.Path] = []
        self._receipt_path: pathlib.Path | None = None

        self.setWindowTitle("Add New Asset")
        self.setMinimumWidth(500)
        self._build_ui()

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        form = QFormLayout()
        form.setLabelAlignment(Qt.AlignmentFlag.AlignRight)

        # Asset ID (auto-generated, read-only)
        self._next_id = self._db.next_asset_id()
        id_lbl = QLabel(self._next_id)
        id_lbl.setStyleSheet("font-weight: bold; color: #555;")
        form.addRow("Asset ID:", id_lbl)

        # Name
        self._name = QLineEdit()
        self._name.setPlaceholderText("e.g. Samsung 65\" TV")
        form.addRow("Name *:", self._name)

        # Date of purchase
        self._date_purchase = QDateEdit()
        self._date_purchase.setCalendarPopup(True)
        self._date_purchase.setDate(QDate.currentDate())
        self._date_purchase.setDisplayFormat("yyyy-MM-dd")
        self._no_date = QCheckBox("Unknown / not entered")
        self._no_date.toggled.connect(self._date_purchase.setDisabled)
        date_row = QWidget()
        date_layout = QHBoxLayout(date_row)
        date_layout.setContentsMargins(0, 0, 0, 0)
        date_layout.addWidget(self._date_purchase)
        date_layout.addWidget(self._no_date)
        form.addRow("Date of Purchase:", date_row)

        # Estimated value
        self._value = QDoubleSpinBox()
        self._value.setPrefix("$ ")
        self._value.setRange(0, 9_999_999)
        self._value.setDecimals(2)
        self._value.setValue(0)
        self._no_value = QCheckBox("Unknown")
        self._no_value.toggled.connect(self._value.setDisabled)
        value_row = QWidget()
        value_layout = QHBoxLayout(value_row)
        value_layout.setContentsMargins(0, 0, 0, 0)
        value_layout.addWidget(self._value)
        value_layout.addWidget(self._no_value)
        form.addRow("Estimated Value:", value_row)

        # Sales receipt
        self._has_receipt = QCheckBox("I have a sales receipt")
        self._btn_receipt = QPushButton("Upload Receipt...")
        self._btn_receipt.setEnabled(False)
        self._receipt_lbl = QLabel()
        self._receipt_lbl.setStyleSheet("color: gray; font-size: 11px;")
        self._has_receipt.toggled.connect(self._btn_receipt.setEnabled)
        self._btn_receipt.clicked.connect(self._pick_receipt)
        receipt_row = QWidget()
        receipt_layout = QHBoxLayout(receipt_row)
        receipt_layout.setContentsMargins(0, 0, 0, 0)
        receipt_layout.addWidget(self._has_receipt)
        receipt_layout.addWidget(self._btn_receipt)
        receipt_layout.addWidget(self._receipt_lbl)
        receipt_layout.addStretch()
        form.addRow("Sales Receipt:", receipt_row)

        layout.addLayout(form)

        # Photos section
        photos_lbl = QLabel("Photos of Asset")
        photos_lbl.setStyleSheet("font-weight: bold; margin-top: 8px;")
        layout.addWidget(photos_lbl)

        self._photo_list = QListWidget()
        self._photo_list.setFixedHeight(120)
        self._photo_list.setSelectionMode(QListWidget.SelectionMode.ExtendedSelection)
        layout.addWidget(self._photo_list)

        photo_btns = QHBoxLayout()
        btn_add_photos = QPushButton("Add Photos...")
        btn_add_photos.clicked.connect(self._pick_photos)
        btn_remove = QPushButton("Remove Selected")
        btn_remove.clicked.connect(self._remove_selected_photos)
        photo_btns.addWidget(btn_add_photos)
        photo_btns.addWidget(btn_remove)
        photo_btns.addStretch()
        layout.addLayout(photo_btns)

        # OK / Cancel
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self._on_accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def _pick_photos(self) -> None:
        exts = " ".join(f"*{e}" for e in sorted(IMAGE_EXTENSIONS))
        paths, _ = QFileDialog.getOpenFileNames(
            self, "Select Photos", "", f"Images ({exts});;All Files (*)"
        )
        for p in paths:
            path = pathlib.Path(p)
            if path not in self._photo_paths:
                self._photo_paths.append(path)
                self._photo_list.addItem(QListWidgetItem(path.name))

    def _remove_selected_photos(self) -> None:
        for item in self._photo_list.selectedItems():
            row = self._photo_list.row(item)
            self._photo_list.takeItem(row)
            self._photo_paths.pop(row)

    def _pick_receipt(self) -> None:
        exts = " ".join(f"*{e}" for e in sorted(RECEIPT_EXTENSIONS))
        path, _ = QFileDialog.getOpenFileName(
            self, "Select Sales Receipt", "", f"Receipt Files ({exts});;All Files (*)"
        )
        if path:
            self._receipt_path = pathlib.Path(path)
            self._receipt_lbl.setText(self._receipt_path.name)

    def _on_accept(self) -> None:
        name = self._name.text().strip()
        if not name:
            QMessageBox.warning(self, "Validation", "Asset name is required.")
            return

        date_purchase: str | None = None
        if not self._no_date.isChecked():
            date_purchase = self._date_purchase.date().toString("yyyy-MM-dd")

        value_estimate: float | None = None
        if not self._no_value.isChecked():
            value_estimate = self._value.value()

        has_receipt = self._has_receipt.isChecked() and self._receipt_path is not None

        asset = Asset(
            id=self._next_id,
            name=name,
            date_purchase=date_purchase,
            value_estimate=value_estimate,
            has_receipt=has_receipt,
            date_added=_now(),
            notes="",
        )

        try:
            self._db.insert_asset(asset)

            if self._photo_paths:
                files = storage.import_files(asset, self._photo_paths, "image")
                for f in files:
                    self._db.insert_asset_file(f)

            if has_receipt and self._receipt_path:
                files = storage.import_files(asset, [self._receipt_path], "receipt")
                for f in files:
                    self._db.insert_asset_file(f)

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to save asset:\n{e}")
            return

        self.accept()
