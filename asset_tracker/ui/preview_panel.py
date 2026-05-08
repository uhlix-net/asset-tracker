from __future__ import annotations
import pathlib

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QFormLayout, QLabel, QPushButton,
    QTextEdit, QScrollArea, QGridLayout, QFrame,
)
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QPixmap, QCursor

from ..models import Asset, AssetFile
from .. import storage


class _ClickableLabel(QLabel):
    def __init__(self, path: pathlib.Path, name: str, encrypted: bool = False, parent=None):
        super().__init__(parent)
        self._path = path
        self._name = name
        self._encrypted = encrypted
        self.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
        self.setToolTip(f"Click to view full size: {name}")

    def mousePressEvent(self, event):
        from .image_viewer import ImageViewer
        dlg = ImageViewer(self._path, self._name, self._encrypted, self.window())
        dlg.exec()


class PreviewPanel(QWidget):
    def __init__(self, db, parent=None) -> None:
        super().__init__(parent)
        self._db = db
        self._asset: Asset | None = None
        self._notes_timer = QTimer(self)
        self._notes_timer.setSingleShot(True)
        self._notes_timer.setInterval(500)
        self._notes_timer.timeout.connect(self._save_notes)
        self._build_ui()
        self.clear()

    def _build_ui(self) -> None:
        outer = QVBoxLayout(self)
        outer.setContentsMargins(8, 8, 8, 8)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        outer.addWidget(scroll)

        container = QWidget()
        scroll.setWidget(container)
        layout = QVBoxLayout(container)
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)

        form = QFormLayout()
        form.setLabelAlignment(Qt.AlignmentFlag.AlignRight)
        self._lbl_id = QLabel()
        self._lbl_name = QLabel()
        self._lbl_name.setWordWrap(True)
        self._lbl_category = QLabel()
        self._lbl_purchase = QLabel()
        self._lbl_value = QLabel()
        self._lbl_current = QLabel()
        self._lbl_serial = QLabel()
        self._lbl_model = QLabel()
        self._lbl_receipt = QLabel()
        self._lbl_receipt.setOpenExternalLinks(False)
        self._lbl_receipt.linkActivated.connect(self._open_receipt)
        self._receipt_path: str | None = None
        self._lbl_added = QLabel()
        form.addRow("Asset ID:", self._lbl_id)
        form.addRow("Name:", self._lbl_name)
        form.addRow("Category:", self._lbl_category)
        form.addRow("Purchase Date:", self._lbl_purchase)
        form.addRow("Purchase Price:", self._lbl_value)
        form.addRow("Current Value:", self._lbl_current)
        form.addRow("Serial Number:", self._lbl_serial)
        form.addRow("Model Number:", self._lbl_model)
        form.addRow("Receipt:", self._lbl_receipt)
        form.addRow("Date Added:", self._lbl_added)
        layout.addLayout(form)

        photos_lbl = QLabel("Photos  (click to view full size)")
        photos_lbl.setStyleSheet("font-weight: bold; margin-top: 8px; color: #1a5276;")
        layout.addWidget(photos_lbl)

        self._photos_grid = QGridLayout()
        self._photos_grid.setSpacing(4)
        photos_container = QWidget()
        photos_container.setLayout(self._photos_grid)
        layout.addWidget(photos_container)

        notes_lbl = QLabel("Notes")
        notes_lbl.setStyleSheet("font-weight: bold; margin-top: 8px;")
        layout.addWidget(notes_lbl)
        self._notes = QTextEdit()
        self._notes.setFixedHeight(80)
        self._notes.textChanged.connect(self._notes_timer.start)
        layout.addWidget(self._notes)

        self._btn_open = QPushButton("Open Asset Folder")
        self._btn_open.clicked.connect(self._open_folder)
        layout.addWidget(self._btn_open)

        self._btn_print = QPushButton("Print This Asset")
        self._btn_print.clicked.connect(self._print_asset)
        layout.addWidget(self._btn_print)
        layout.addStretch()

    def show_asset(self, asset: Asset, files: list[AssetFile] | None = None) -> None:
        self._asset = asset
        if files is None:
            files = self._db.get_asset_files(asset.id)

        self._lbl_id.setText(asset.id)
        self._lbl_name.setText(asset.name)
        self._lbl_category.setText(asset.category or "—")
        self._lbl_purchase.setText(asset.date_purchase or "—")
        self._lbl_value.setText(asset.value_display)
        self._lbl_current.setText(asset.current_value_display)
        self._lbl_serial.setText(asset.serial_number or "—")
        self._lbl_model.setText(asset.model_number or "—")
        receipt_file = next((f for f in files if f.file_type == "receipt"), None)
        if asset.has_receipt and receipt_file:
            self._receipt_path = str(storage.get_stored_path(asset, receipt_file))
            self._lbl_receipt.setText(
                '<a href="open" style="color: #27ae60; text-decoration: none;">'
                'Receipt on file <span style="color: #27ae60; font-weight: bold;">✓</span></a>'
            )
        else:
            self._receipt_path = None
            self._lbl_receipt.setText("No receipt")
        self._lbl_added.setText(asset.date_added[:10])

        self._notes.blockSignals(True)
        self._notes.setPlainText(asset.notes)
        self._notes.blockSignals(False)
        self._notes.setEnabled(True)
        self._btn_open.setEnabled(True)
        self._btn_print.setEnabled(True)

        self._load_photos(asset, files)

    def _load_photos(self, asset: Asset, files: list[AssetFile]) -> None:
        while self._photos_grid.count():
            item = self._photos_grid.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        images = [f for f in files if f.file_type == "image"]
        if not images:
            lbl = QLabel("No photos")
            lbl.setStyleSheet("color: gray;")
            self._photos_grid.addWidget(lbl, 0, 0)
            return

        for i, af in enumerate(images):
            path = storage.get_stored_path(asset, af)
            pixmap = storage.generate_thumbnail(path, size=(160, 160), encrypted=af.encrypted)
            lbl = _ClickableLabel(path, af.file_name, af.encrypted)
            lbl.setFixedSize(164, 164)
            lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
            lbl.setStyleSheet("border: 1px solid #aaa;")
            if pixmap:
                lbl.setPixmap(
                    pixmap.scaled(160, 160,
                                  Qt.AspectRatioMode.KeepAspectRatio,
                                  Qt.TransformationMode.SmoothTransformation)
                )
            else:
                lbl.setText(af.file_name)
                lbl.setWordWrap(True)
            row, col = divmod(i, 2)
            self._photos_grid.addWidget(lbl, row, col)

    def clear(self) -> None:
        self._asset = None
        self._receipt_path = None
        for lbl in (self._lbl_id, self._lbl_name, self._lbl_category,
                    self._lbl_purchase, self._lbl_value, self._lbl_current,
                    self._lbl_serial, self._lbl_model, self._lbl_receipt,
                    self._lbl_added):
            lbl.clear()
        self._notes.blockSignals(True)
        self._notes.clear()
        self._notes.blockSignals(False)
        self._notes.setEnabled(False)
        self._btn_open.setEnabled(False)
        self._btn_print.setEnabled(False)
        while self._photos_grid.count():
            item = self._photos_grid.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

    def _save_notes(self) -> None:
        if self._asset:
            self._db.update_notes(self._asset.id, self._notes.toPlainText())

    def _print_asset(self) -> None:
        if not self._asset:
            return
        import os, subprocess
        from PyQt6.QtWidgets import QFileDialog, QMessageBox
        from ..ui.insurer_info_dialog import InsurerInfoDialog, load_insurer_info
        from .. import report

        dest, _ = QFileDialog.getSaveFileName(
            self,
            f"Save Asset Record — {self._asset.name}",
            f"Asset_{self._asset.id}_{self._asset.name[:30].replace(' ', '_')}.pdf",
            "PDF Files (*.pdf)",
        )
        if not dest:
            return

        insurer_info = load_insurer_info()
        files = self._db.get_asset_files(self._asset.id)
        try:
            report.generate_single_asset_report(
                self._asset, files, dest, insurer_info
            )
            reply = QMessageBox.information(
                self, "Asset Record Saved", f"Saved to:\n{dest}\n\nOpen file?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            )
            if reply == QMessageBox.StandardButton.Yes:
                if os.name == "nt":
                    os.startfile(dest)
                else:
                    subprocess.Popen(["xdg-open", dest])
        except Exception as e:
            QMessageBox.critical(self, "Print Failed", str(e))

    def _open_receipt(self, _href: str) -> None:
        if not self._receipt_path:
            return
        import os, subprocess, pathlib
        p = pathlib.Path(self._receipt_path)
        if p.exists():
            if os.name == "nt":
                os.startfile(str(p))
            else:
                subprocess.Popen(["xdg-open", str(p)])

    def _open_folder(self) -> None:
        if not self._asset:
            return
        import os, subprocess
        d = storage.asset_dir(self._asset)
        if d.exists():
            if os.name == "nt":
                os.startfile(str(d))
            else:
                subprocess.Popen(["xdg-open", str(d)])
