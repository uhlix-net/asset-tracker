from __future__ import annotations
import pathlib

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QFormLayout, QLabel, QPushButton,
    QTextEdit, QScrollArea, QGridLayout, QFrame, QSizePolicy,
)
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QPixmap

from ..models import Asset, AssetFile
from .. import storage


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

        # Metadata form
        form = QFormLayout()
        form.setLabelAlignment(Qt.AlignmentFlag.AlignRight)
        self._lbl_id = QLabel()
        self._lbl_name = QLabel()
        self._lbl_name.setWordWrap(True)
        self._lbl_purchase = QLabel()
        self._lbl_value = QLabel()
        self._lbl_receipt = QLabel()
        self._lbl_added = QLabel()
        form.addRow("Asset ID:", self._lbl_id)
        form.addRow("Name:", self._lbl_name)
        form.addRow("Purchase Date:", self._lbl_purchase)
        form.addRow("Est. Value:", self._lbl_value)
        form.addRow("Receipt:", self._lbl_receipt)
        form.addRow("Date Added:", self._lbl_added)
        layout.addLayout(form)

        # Photos section
        self._photos_label = QLabel("Photos")
        self._photos_label.setStyleSheet("font-weight: bold; margin-top: 8px;")
        layout.addWidget(self._photos_label)

        self._photos_grid = QGridLayout()
        self._photos_grid.setSpacing(4)
        photos_container = QWidget()
        photos_container.setLayout(self._photos_grid)
        layout.addWidget(photos_container)

        # Notes
        notes_lbl = QLabel("Notes")
        notes_lbl.setStyleSheet("font-weight: bold; margin-top: 8px;")
        layout.addWidget(notes_lbl)
        self._notes = QTextEdit()
        self._notes.setFixedHeight(80)
        self._notes.textChanged.connect(self._notes_timer.start)
        layout.addWidget(self._notes)

        # Actions
        self._btn_open = QPushButton("Open Asset Folder")
        self._btn_open.clicked.connect(self._open_folder)
        layout.addWidget(self._btn_open)

        layout.addStretch()

    def show_asset(self, asset: Asset, files: list[AssetFile] | None = None) -> None:
        self._asset = asset
        if files is None:
            files = self._db.get_asset_files(asset.id)

        self._lbl_id.setText(asset.id)
        self._lbl_name.setText(asset.name)
        self._lbl_purchase.setText(asset.date_purchase or "—")
        self._lbl_value.setText(asset.value_display)
        self._lbl_receipt.setText("Receipt on file ✓" if asset.has_receipt else "No receipt")
        self._lbl_added.setText(asset.date_added[:10])

        self._notes.blockSignals(True)
        self._notes.setPlainText(asset.notes)
        self._notes.blockSignals(False)

        self._btn_open.setEnabled(True)
        self._notes.setEnabled(True)

        self._load_photos(asset, files)

    def _load_photos(self, asset: Asset, files: list[AssetFile]) -> None:
        # Clear existing thumbnails
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
            pixmap = storage.generate_thumbnail(path, size=(160, 160))
            lbl = QLabel()
            lbl.setFixedSize(164, 164)
            lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
            lbl.setStyleSheet("border: 1px solid #ccc;")
            lbl.setToolTip(af.file_name)
            if pixmap:
                lbl.setPixmap(
                    pixmap.scaled(
                        160, 160,
                        Qt.AspectRatioMode.KeepAspectRatio,
                        Qt.TransformationMode.SmoothTransformation,
                    )
                )
            else:
                lbl.setText(af.file_name)
                lbl.setWordWrap(True)
            row, col = divmod(i, 2)
            self._photos_grid.addWidget(lbl, row, col)

    def clear(self) -> None:
        self._asset = None
        for lbl in (
            self._lbl_id, self._lbl_name, self._lbl_purchase,
            self._lbl_value, self._lbl_receipt, self._lbl_added,
        ):
            lbl.clear()
        self._notes.blockSignals(True)
        self._notes.clear()
        self._notes.blockSignals(False)
        self._notes.setEnabled(False)
        self._btn_open.setEnabled(False)
        while self._photos_grid.count():
            item = self._photos_grid.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

    def _save_notes(self) -> None:
        if self._asset:
            self._db.update_notes(self._asset.id, self._notes.toPlainText())

    def _open_folder(self) -> None:
        if not self._asset:
            return
        import os
        d = storage.asset_dir(self._asset)
        if d.exists():
            if os.name == "nt":
                os.startfile(str(d))
            else:
                import subprocess
                subprocess.Popen(["xdg-open", str(d)])
