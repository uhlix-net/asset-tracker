from __future__ import annotations
import pathlib

from PyQt6.QtWidgets import QDialog, QVBoxLayout, QLabel, QScrollArea, QDialogButtonBox
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QPixmap


class ImageViewer(QDialog):
    def __init__(self, image_path: pathlib.Path, title: str = "", parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle(title or image_path.name)
        self.resize(800, 600)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        scroll = QScrollArea()
        scroll.setAlignment(Qt.AlignmentFlag.AlignCenter)
        scroll.setWidgetResizable(False)

        self._label = QLabel()
        self._label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        scroll.setWidget(self._label)
        layout.addWidget(scroll)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Close)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self._load(image_path)

    def _load(self, path: pathlib.Path) -> None:
        pixmap = QPixmap(str(path))
        if pixmap.isNull():
            self._label.setText(f"Cannot display: {path.name}")
            return
        # Scale to fit screen while keeping aspect ratio
        screen = self.screen().availableGeometry()
        max_w = int(screen.width() * 0.85)
        max_h = int(screen.height() * 0.80)
        scaled = pixmap.scaled(
            max_w, max_h,
            Qt.AspectRatioMode.KeepAspectRatio,
            Qt.TransformationMode.SmoothTransformation,
        )
        self._label.setPixmap(scaled)
        self._label.resize(scaled.size())
