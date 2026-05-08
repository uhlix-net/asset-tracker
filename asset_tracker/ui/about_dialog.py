from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QLabel, QDialogButtonBox,
    QFormLayout, QWidget, QFrame,
)
from PyQt6.QtCore import Qt
from ..config import APP_NAME, APP_VERSION, DB_PATH, ASSETS_DIR


class AboutDialog(QDialog):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle(f"About {APP_NAME}")
        self.setMinimumWidth(480)

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

        # Divider
        line = QFrame()
        line.setFrameShape(QFrame.Shape.HLine)
        line.setStyleSheet("color: #d0d0d0;")

        # Data paths
        paths_label = QLabel("Data Locations")
        paths_label.setStyleSheet("font-weight: bold; color: #333;")

        form = QFormLayout()
        form.setLabelAlignment(Qt.AlignmentFlag.AlignRight)
        form.setHorizontalSpacing(12)

        db_lbl = QLabel(str(DB_PATH))
        db_lbl.setStyleSheet("color: #555; font-family: monospace;")
        db_lbl.setWordWrap(True)
        db_lbl.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)

        assets_lbl = QLabel(str(ASSETS_DIR))
        assets_lbl.setStyleSheet("color: #555; font-family: monospace;")
        assets_lbl.setWordWrap(True)
        assets_lbl.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)

        form.addRow("Database:", db_lbl)
        form.addRow("Assets folder:", assets_lbl)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok)
        buttons.accepted.connect(self.accept)

        layout.addWidget(title)
        layout.addWidget(version)
        layout.addSpacing(4)
        layout.addWidget(desc)
        layout.addSpacing(4)
        layout.addWidget(line)
        layout.addWidget(paths_label)
        layout.addLayout(form)
        layout.addSpacing(4)
        layout.addWidget(buttons)
