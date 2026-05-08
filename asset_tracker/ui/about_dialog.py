from PyQt6.QtWidgets import QDialog, QVBoxLayout, QLabel, QDialogButtonBox
from PyQt6.QtCore import Qt
from ..config import APP_NAME, APP_VERSION


class AboutDialog(QDialog):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle(f"About {APP_NAME}")
        self.setFixedWidth(360)

        layout = QVBoxLayout(self)
        layout.setSpacing(8)

        title = QLabel(APP_NAME)
        title.setStyleSheet("font-size: 20px; font-weight: bold;")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)

        version = QLabel(f"Version {APP_VERSION}")
        version.setAlignment(Qt.AlignmentFlag.AlignCenter)
        version.setStyleSheet("color: #555;")

        desc = QLabel(
            "A personal asset tracking application for documenting\n"
            "household items for insurance purposes."
        )
        desc.setAlignment(Qt.AlignmentFlag.AlignCenter)
        desc.setWordWrap(True)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok)
        buttons.accepted.connect(self.accept)

        layout.addWidget(title)
        layout.addWidget(version)
        layout.addSpacing(8)
        layout.addWidget(desc)
        layout.addSpacing(8)
        layout.addWidget(buttons)
