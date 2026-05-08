from __future__ import annotations
import pathlib
from datetime import datetime, timezone

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QFormLayout, QLineEdit, QDateEdit, QDoubleSpinBox,
    QCheckBox, QPushButton, QListWidget, QListWidgetItem, QDialogButtonBox,
    QLabel, QHBoxLayout, QFileDialog, QWidget, QMessageBox, QComboBox,
)
from PyQt6.QtCore import Qt, QDate

from ..database import Database
from ..models import Asset
from ..config import IMAGE_EXTENSIONS, RECEIPT_EXTENSIONS, ASSET_CATEGORIES
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
        self.setMinimumWidth(520)
        self._build_ui()

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        form = QFormLayout()
        form.setLabelAlignment(Qt.AlignmentFlag.AlignRight)

        self._next_id = self._db.next_asset_id()
        id_lbl = QLabel(self._next_id)
        id_lbl.setStyleSheet("font-weight: bold; color: #555;")
        form.addRow("Asset ID:", id_lbl)

        self._name = QLineEdit()
        self._name.setPlaceholderText("e.g. Samsung 65\" TV")
        form.addRow("Name *:", self._name)

        self._category = QComboBox()
        self._category.addItem("— Select Category —", "")
        for cat in ASSET_CATEGORIES:
            self._category.addItem(cat, cat)
        form.addRow("Category:", self._category)

        self._serial = QLineEdit()
        self._serial.setPlaceholderText("Optional")
        form.addRow("Serial Number:", self._serial)

        self._model = QLineEdit()
        self._model.setPlaceholderText("Optional")
        form.addRow("Model Number:", self._model)

        self._date_purchase = QDateEdit()
        self._date_purchase.setCalendarPopup(True)
        self._date_purchase.setDate(QDate.currentDate())
        self._date_purchase.setDisplayFormat("yyyy-MM-dd")
        self._no_date = QCheckBox("Unknown")
        self._no_date.toggled.connect(self._date_purchase.setDisabled)
        date_row = QWidget()
        hl = QHBoxLayout(date_row)
        hl.setContentsMargins(0, 0, 0, 0)
        hl.addWidget(self._date_purchase)
        hl.addWidget(self._no_date)
        form.addRow("Purchase Date:", date_row)

        self._value = QDoubleSpinBox()
        self._value.setPrefix("$ ")
        self._value.setRange(0, 9_999_999)
        self._value.setDecimals(2)
        self._no_value = QCheckBox("Unknown")
        self._no_value.toggled.connect(self._value.setDisabled)
        val_row = QWidget()
        hl2 = QHBoxLayout(val_row)
        hl2.setContentsMargins(0, 0, 0, 0)
        hl2.addWidget(self._value)
        hl2.addWidget(self._no_value)
        form.addRow("Purchase Value:", val_row)

        self._current_value = QDoubleSpinBox()
        self._current_value.setPrefix("$ ")
        self._current_value.setRange(0, 9_999_999)
        self._current_value.setDecimals(2)
        self._no_current = QCheckBox("Unknown")
        self._no_current.setChecked(True)
        self._current_value.setEnabled(False)
        self._no_current.toggled.connect(self._current_value.setDisabled)
        cur_row = QWidget()
        hl3 = QHBoxLayout(cur_row)
        hl3.setContentsMargins(0, 0, 0, 0)
        hl3.addWidget(self._current_value)
        hl3.addWidget(self._no_current)
        form.addRow("Current Value:", cur_row)

        self._has_receipt = QCheckBox("I have a sales receipt")
        self._btn_receipt = QPushButton("Upload Receipt...")
        self._btn_receipt.setEnabled(False)
        self._receipt_lbl = QLabel()
        self._receipt_lbl.setStyleSheet("color: gray; font-size: 11px;")
        self._has_receipt.toggled.connect(self._btn_receipt.setEnabled)
        self._btn_receipt.clicked.connect(self._pick_receipt)
        rec_row = QWidget()
        hl4 = QHBoxLayout(rec_row)
        hl4.setContentsMargins(0, 0, 0, 0)
        hl4.addWidget(self._has_receipt)
        hl4.addWidget(self._btn_receipt)
        hl4.addWidget(self._receipt_lbl)
        hl4.addStretch()
        form.addRow("Sales Receipt:", rec_row)

        layout.addLayout(form)

        photos_lbl = QLabel("Photos of Asset")
        photos_lbl.setStyleSheet("font-weight: bold; margin-top: 8px;")
        layout.addWidget(photos_lbl)

        self._photo_list = QListWidget()
        self._photo_list.setFixedHeight(100)
        self._photo_list.setSelectionMode(QListWidget.SelectionMode.ExtendedSelection)
        layout.addWidget(self._photo_list)

        btns = QHBoxLayout()
        btn_add = QPushButton("Add Photos...")
        btn_add.clicked.connect(self._pick_photos)
        btn_rm = QPushButton("Remove Selected")
        btn_rm.clicked.connect(self._remove_photos)
        btns.addWidget(btn_add)
        btns.addWidget(btn_rm)
        btns.addStretch()
        layout.addLayout(btns)

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

    def _remove_photos(self) -> None:
        for item in reversed(self._photo_list.selectedItems()):
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

        asset = Asset(
            id=self._next_id,
            name=name,
            category=self._category.currentData() or "",
            date_purchase=None if self._no_date.isChecked()
                          else self._date_purchase.date().toString("yyyy-MM-dd"),
            value_estimate=None if self._no_value.isChecked() else self._value.value(),
            current_value=None if self._no_current.isChecked() else self._current_value.value(),
            serial_number=self._serial.text().strip(),
            model_number=self._model.text().strip(),
            has_receipt=self._has_receipt.isChecked() and self._receipt_path is not None,
            date_added=_now(),
        )
        try:
            self._db.insert_asset(asset)
            if self._photo_paths:
                for f in storage.import_files(asset, self._photo_paths, "image"):
                    self._db.insert_asset_file(f)
            if asset.has_receipt and self._receipt_path:
                for f in storage.import_files(asset, [self._receipt_path], "receipt"):
                    self._db.insert_asset_file(f)
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to save asset:\n{e}")
            return
        self.accept()
