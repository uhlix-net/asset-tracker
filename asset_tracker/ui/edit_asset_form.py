from __future__ import annotations
import pathlib

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QFormLayout, QLineEdit, QDateEdit, QDoubleSpinBox,
    QCheckBox, QPushButton, QListWidget, QListWidgetItem, QDialogButtonBox,
    QLabel, QHBoxLayout, QFileDialog, QWidget, QMessageBox, QAbstractItemView,
)
from PyQt6.QtCore import Qt, QDate

from ..database import Database
from ..models import Asset, AssetFile
from ..config import IMAGE_EXTENSIONS, RECEIPT_EXTENSIONS
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
        self.setMinimumWidth(520)
        self._build_ui()

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        form = QFormLayout()
        form.setLabelAlignment(Qt.AlignmentFlag.AlignRight)

        # Asset ID (read-only)
        id_lbl = QLabel(self._asset.id)
        id_lbl.setStyleSheet("font-weight: bold; color: #555;")
        form.addRow("Asset ID:", id_lbl)

        # Name
        self._name = QLineEdit(self._asset.name)
        form.addRow("Name *:", self._name)

        # Date of purchase
        self._no_date = QCheckBox("Unknown / not entered")
        self._date_purchase = QDateEdit()
        self._date_purchase.setCalendarPopup(True)
        self._date_purchase.setDisplayFormat("yyyy-MM-dd")
        if self._asset.date_purchase:
            self._date_purchase.setDate(QDate.fromString(self._asset.date_purchase, "yyyy-MM-dd"))
            self._no_date.setChecked(False)
        else:
            self._date_purchase.setDate(QDate.currentDate())
            self._date_purchase.setEnabled(False)
            self._no_date.setChecked(True)
        self._no_date.toggled.connect(self._date_purchase.setDisabled)
        date_row = QWidget()
        date_layout = QHBoxLayout(date_row)
        date_layout.setContentsMargins(0, 0, 0, 0)
        date_layout.addWidget(self._date_purchase)
        date_layout.addWidget(self._no_date)
        form.addRow("Date of Purchase:", date_row)

        # Estimated value
        self._no_value = QCheckBox("Unknown")
        self._value = QDoubleSpinBox()
        self._value.setPrefix("$ ")
        self._value.setRange(0, 9_999_999)
        self._value.setDecimals(2)
        if self._asset.value_estimate is not None:
            self._value.setValue(self._asset.value_estimate)
            self._no_value.setChecked(False)
        else:
            self._value.setValue(0)
            self._value.setEnabled(False)
            self._no_value.setChecked(True)
        self._no_value.toggled.connect(self._value.setDisabled)
        value_row = QWidget()
        value_layout = QHBoxLayout(value_row)
        value_layout.setContentsMargins(0, 0, 0, 0)
        value_layout.addWidget(self._value)
        value_layout.addWidget(self._no_value)
        form.addRow("Estimated Value:", value_row)

        # Sales receipt
        self._has_receipt = QCheckBox("I have a sales receipt")
        self._has_receipt.setChecked(self._asset.has_receipt)
        self._btn_receipt = QPushButton("Replace Receipt...")
        self._btn_receipt.setEnabled(self._asset.has_receipt)
        self._receipt_lbl = QLabel()
        self._receipt_lbl.setStyleSheet("color: gray; font-size: 11px;")
        existing_receipt = next(
            (f for f in self._existing_files if f.file_type == "receipt"), None
        )
        if existing_receipt:
            self._receipt_lbl.setText(f"Current: {existing_receipt.file_name}")
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

        # Existing photos
        existing_lbl = QLabel("Existing Photos  (select rows + click Remove to delete)")
        existing_lbl.setStyleSheet("font-weight: bold; margin-top: 8px;")
        layout.addWidget(existing_lbl)

        self._existing_list = QListWidget()
        self._existing_list.setFixedHeight(100)
        self._existing_list.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        existing_images = [f for f in self._existing_files if f.file_type == "image"]
        for af in existing_images:
            item = QListWidgetItem(af.file_name)
            item.setData(Qt.ItemDataRole.UserRole, af)
            self._existing_list.addItem(item)
        layout.addWidget(self._existing_list)

        btn_remove_existing = QPushButton("Remove Selected Photos")
        btn_remove_existing.clicked.connect(self._remove_existing_photos)
        layout.addWidget(btn_remove_existing)

        # Add new photos
        new_lbl = QLabel("Add New Photos")
        new_lbl.setStyleSheet("font-weight: bold; margin-top: 8px;")
        layout.addWidget(new_lbl)

        self._new_photo_list = QListWidget()
        self._new_photo_list.setFixedHeight(80)
        self._new_photo_list.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        layout.addWidget(self._new_photo_list)

        new_photo_btns = QHBoxLayout()
        btn_add = QPushButton("Add Photos...")
        btn_add.clicked.connect(self._pick_photos)
        btn_remove_new = QPushButton("Remove Selected")
        btn_remove_new.clicked.connect(self._remove_new_photos)
        new_photo_btns.addWidget(btn_add)
        new_photo_btns.addWidget(btn_remove_new)
        new_photo_btns.addStretch()
        layout.addLayout(new_photo_btns)

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
            self, "Select Sales Receipt", "", f"Receipt Files ({exts});;All Files (*)"
        )
        if path:
            self._new_receipt_path = pathlib.Path(path)
            self._receipt_lbl.setText(f"New: {self._new_receipt_path.name}")

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

        has_receipt = self._has_receipt.isChecked()

        # Build updated asset (preserve id and date_added)
        updated = Asset(
            id=self._asset.id,
            name=name,
            date_purchase=date_purchase,
            value_estimate=value_estimate,
            has_receipt=has_receipt,
            date_added=self._asset.date_added,
            notes=self._asset.notes,
        )

        try:
            # Remove deleted existing photos from disk and DB
            for af in self._files_to_remove:
                path = storage.get_stored_path(self._asset, af)
                path.unlink(missing_ok=True)
                if af.id is not None:
                    self._db.delete_asset_file(af.id)

            # If the asset directory was renamed (name changed), rename it
            old_dir = storage.asset_dir(self._asset)
            new_dir = storage.asset_dir(updated)
            if old_dir != new_dir and old_dir.exists():
                old_dir.rename(new_dir)

            # Save new photos
            if self._new_photo_paths:
                files = storage.import_files(updated, self._new_photo_paths, "image")
                for f in files:
                    self._db.insert_asset_file(f)

            # Replace receipt if a new one was selected
            if self._new_receipt_path:
                # Remove old receipt file(s)
                old_receipts = [
                    f for f in self._existing_files if f.file_type == "receipt"
                ]
                for af in old_receipts:
                    path = storage.get_stored_path(self._asset, af)
                    path.unlink(missing_ok=True)
                    if af.id is not None:
                        self._db.delete_asset_file(af.id)
                files = storage.import_files(updated, [self._new_receipt_path], "receipt")
                for f in files:
                    self._db.insert_asset_file(f)

            self._db.update_asset(updated)

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to save changes:\n{e}")
            return

        self.accept()
