from __future__ import annotations
import pathlib

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QFormLayout, QLineEdit, QDateEdit, QDoubleSpinBox,
    QCheckBox, QPushButton, QListWidget, QListWidgetItem, QDialogButtonBox,
    QLabel, QHBoxLayout, QFileDialog, QWidget, QMessageBox,
    QAbstractItemView, QComboBox,
)
from PyQt6.QtCore import Qt, QDate

from ..database import Database
from ..models import Asset, AssetFile
from ..config import IMAGE_EXTENSIONS, RECEIPT_EXTENSIONS, ASSET_CATEGORIES
from .. import storage


class EditAssetFormDialog(QDialog):
    def __init__(self, db: Database, asset: Asset, parent=None) -> None:
        super().__init__(parent)
        self._db = db
        self._asset = asset
        self._existing_files: list[AssetFile] = db.get_asset_files(asset.id)
        self._files_to_remove: list[AssetFile] = []
        self._new_photo_paths: list[pathlib.Path] = []
        self._new_receipt_path: pathlib.Path | None = None
        self.setWindowTitle(f"Edit Asset — {asset.id}")
        self.setMinimumWidth(540)
        self._build_ui()

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        form = QFormLayout()
        form.setLabelAlignment(Qt.AlignmentFlag.AlignRight)

        id_lbl = QLabel(self._asset.id)
        id_lbl.setStyleSheet("font-weight: bold; color: #555;")
        form.addRow("Asset ID:", id_lbl)

        self._name = QLineEdit(self._asset.name)
        form.addRow("Name *:", self._name)

        self._category = QComboBox()
        self._category.addItem("— Select Category —", "")
        for cat in ASSET_CATEGORIES:
            self._category.addItem(cat, cat)
        idx = self._category.findData(self._asset.category)
        if idx >= 0:
            self._category.setCurrentIndex(idx)
        form.addRow("Category:", self._category)

        self._serial = QLineEdit(self._asset.serial_number)
        form.addRow("Serial Number:", self._serial)

        self._model = QLineEdit(self._asset.model_number)
        form.addRow("Model Number:", self._model)

        self._no_date = QCheckBox("Unknown")
        self._date_purchase = QDateEdit()
        self._date_purchase.setCalendarPopup(True)
        self._date_purchase.setDisplayFormat("yyyy-MM-dd")
        if self._asset.date_purchase:
            self._date_purchase.setDate(
                QDate.fromString(self._asset.date_purchase, "yyyy-MM-dd"))
        else:
            self._date_purchase.setDate(QDate.currentDate())
            self._date_purchase.setEnabled(False)
            self._no_date.setChecked(True)
        self._no_date.toggled.connect(self._date_purchase.setDisabled)
        date_row = QWidget()
        hl = QHBoxLayout(date_row)
        hl.setContentsMargins(0, 0, 0, 0)
        hl.addWidget(self._date_purchase)
        hl.addWidget(self._no_date)
        form.addRow("Purchase Date:", date_row)

        self._no_value = QCheckBox("Unknown")
        self._value = QDoubleSpinBox()
        self._value.setPrefix("$ ")
        self._value.setRange(0, 9_999_999)
        self._value.setDecimals(2)
        if self._asset.value_estimate is not None:
            self._value.setValue(self._asset.value_estimate)
        else:
            self._value.setEnabled(False)
            self._no_value.setChecked(True)
        self._no_value.toggled.connect(self._value.setDisabled)
        val_row = QWidget()
        hl2 = QHBoxLayout(val_row)
        hl2.setContentsMargins(0, 0, 0, 0)
        hl2.addWidget(self._value)
        hl2.addWidget(self._no_value)
        form.addRow("Purchase Price:", val_row)

        self._no_current = QCheckBox("Unknown")
        self._current_value = QDoubleSpinBox()
        self._current_value.setPrefix("$ ")
        self._current_value.setRange(0, 9_999_999)
        self._current_value.setDecimals(2)
        if self._asset.current_value is not None:
            self._current_value.setValue(self._asset.current_value)
        else:
            self._current_value.setEnabled(False)
            self._no_current.setChecked(True)
        self._no_current.toggled.connect(self._current_value.setDisabled)
        cur_row = QWidget()
        hl3 = QHBoxLayout(cur_row)
        hl3.setContentsMargins(0, 0, 0, 0)
        hl3.addWidget(self._current_value)
        hl3.addWidget(self._no_current)
        form.addRow("Current Value:", cur_row)

        self._has_receipt = QCheckBox("I have a sales receipt")
        self._has_receipt.setChecked(self._asset.has_receipt)
        self._btn_receipt = QPushButton("Replace Receipt...")
        self._btn_receipt.setEnabled(self._asset.has_receipt)
        self._receipt_lbl = QLabel()
        self._receipt_lbl.setStyleSheet("color: gray; font-size: 11px;")
        existing_receipt = next(
            (f for f in self._existing_files if f.file_type == "receipt"), None)
        if existing_receipt:
            self._receipt_lbl.setText(f"Current: {existing_receipt.file_name}")
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

        existing_lbl = QLabel("Existing Photos  (select + Remove to delete)")
        existing_lbl.setStyleSheet("font-weight: bold; margin-top: 8px;")
        layout.addWidget(existing_lbl)

        self._existing_list = QListWidget()
        self._existing_list.setFixedHeight(90)
        self._existing_list.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        for af in [f for f in self._existing_files if f.file_type == "image"]:
            item = QListWidgetItem(af.file_name)
            item.setData(Qt.ItemDataRole.UserRole, af)
            self._existing_list.addItem(item)
        layout.addWidget(self._existing_list)

        btn_rm_ex = QPushButton("Remove Selected Photos")
        btn_rm_ex.clicked.connect(self._remove_existing_photos)
        layout.addWidget(btn_rm_ex)

        new_lbl = QLabel("Add New Photos")
        new_lbl.setStyleSheet("font-weight: bold; margin-top: 8px;")
        layout.addWidget(new_lbl)

        self._new_photo_list = QListWidget()
        self._new_photo_list.setFixedHeight(70)
        self._new_photo_list.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        layout.addWidget(self._new_photo_list)

        btns = QHBoxLayout()
        btn_add = QPushButton("Add Photos...")
        btn_add.clicked.connect(self._pick_photos)
        btn_rm = QPushButton("Remove Selected")
        btn_rm.clicked.connect(self._remove_new_photos)
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
            self, "Select Photos", "", f"Images ({exts});;All Files (*)")
        for p in paths:
            path = pathlib.Path(p)
            if path not in self._new_photo_paths:
                self._new_photo_paths.append(path)
                self._new_photo_list.addItem(QListWidgetItem(path.name))

    def _remove_new_photos(self) -> None:
        for item in reversed(self._new_photo_list.selectedItems()):
            row = self._new_photo_list.row(item)
            self._new_photo_list.takeItem(row)
            self._new_photo_paths.pop(row)

    def _remove_existing_photos(self) -> None:
        for item in self._existing_list.selectedItems():
            af: AssetFile = item.data(Qt.ItemDataRole.UserRole)
            self._files_to_remove.append(af)
            self._existing_list.takeItem(self._existing_list.row(item))

    def _pick_receipt(self) -> None:
        exts = " ".join(f"*{e}" for e in sorted(RECEIPT_EXTENSIONS))
        path, _ = QFileDialog.getOpenFileName(
            self, "Select Sales Receipt", "", f"Receipt Files ({exts});;All Files (*)")
        if path:
            self._new_receipt_path = pathlib.Path(path)
            self._receipt_lbl.setText(f"New: {self._new_receipt_path.name}")

    def _on_accept(self) -> None:
        name = self._name.text().strip()
        if not name:
            QMessageBox.warning(self, "Validation", "Asset name is required.")
            return

        updated = Asset(
            id=self._asset.id,
            name=name,
            category=self._category.currentData() or "",
            date_purchase=None if self._no_date.isChecked()
                          else self._date_purchase.date().toString("yyyy-MM-dd"),
            value_estimate=None if self._no_value.isChecked() else self._value.value(),
            current_value=None if self._no_current.isChecked() else self._current_value.value(),
            serial_number=self._serial.text().strip(),
            model_number=self._model.text().strip(),
            has_receipt=self._has_receipt.isChecked(),
            date_added=self._asset.date_added,
            notes=self._asset.notes,
        )
        try:
            for af in self._files_to_remove:
                storage.get_stored_path(self._asset, af).unlink(missing_ok=True)
                if af.id is not None:
                    self._db.delete_asset_file(af.id)

            old_dir = storage.asset_dir(self._asset)
            new_dir = storage.asset_dir(updated)
            if old_dir != new_dir and old_dir.exists():
                old_dir.rename(new_dir)

            if self._new_photo_paths:
                for f in storage.import_files(updated, self._new_photo_paths, "image"):
                    self._db.insert_asset_file(f)

            if self._new_receipt_path:
                for af in [f for f in self._existing_files if f.file_type == "receipt"]:
                    storage.get_stored_path(self._asset, af).unlink(missing_ok=True)
                    if af.id is not None:
                        self._db.delete_asset_file(af.id)
                for f in storage.import_files(updated, [self._new_receipt_path], "receipt"):
                    self._db.insert_asset_file(f)

            self._db.update_asset(updated)
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to save changes:\n{e}")
            return
        self.accept()
