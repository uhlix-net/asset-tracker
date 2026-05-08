from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QLabel, QDialogButtonBox,
    QScrollArea, QWidget, QFrame,
)
from PyQt6.QtCore import Qt
from ..config import APP_NAME

# Each entry: (version, title, [bullet points])
HISTORY = [
    (
        "1.0.5",
        "Installer and Help improvements",
        [
            "Removed version number from installer dialog titles",
            "Added Update History dialog (Help menu)",
        ],
    ),
    (
        "1.0.4",
        "Python installer progress bar",
        [
            "Python installation now shows a progress bar dialog instead "
            "of running invisibly in the background",
        ],
    ),
    (
        "1.0.3",
        "About page data paths",
        [
            "About dialog now displays the full path to the database file "
            "and the assets folder",
            "Paths are selectable by mouse for easy copying",
        ],
    ),
    (
        "1.0.2",
        "Layout improvements",
        [
            "Asset table and preview panel each take 50% of the window width "
            "and scale together when the window is resized",
            "Divider line below the toolbar always spans the full window width",
            "Toolbar, table, and preview panel anchored to the top — "
            "enlarging the window vertically adds empty space below",
        ],
    ),
    (
        "1.0.0",
        "Initial release",
        [
            "Track household assets with photos, sales receipts, purchase date, "
            "purchase value, and current/appraised value",
            "Assign assets to categories (Living Room, Kitchen, Garage, etc.) "
            "with serial number and model number fields",
            "Search assets by name and filter by category",
            "Preview panel shows thumbnails — click any photo to view full size",
            "Right-click menu and bulk multi-select delete",
            "Duplicate an existing asset to quickly add similar items",
            "Encrypted ZIP backup (AES-256) with password, and restore from backup",
            "PDF report with title page, table of contents, and per-asset pages "
            "grouped by category — suitable for insurance claims",
            "CSV export compatible with Excel",
            "Auto-backup prompt on exit (optional, toggleable in File menu)",
            "Live status bar showing asset count and total purchase/current values",
            "Windows installer with automatic Python detection and installation",
        ],
    ),
]


class UpdateHistoryDialog(QDialog):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle(f"{APP_NAME} — Update History")
        self.setMinimumWidth(520)
        self.setMinimumHeight(480)
        self.resize(560, 560)

        outer = QVBoxLayout(self)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        outer.addWidget(scroll)

        container = QWidget()
        scroll.setWidget(container)
        layout = QVBoxLayout(container)
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        layout.setSpacing(16)

        for version, title, bullets in HISTORY:
            # Version heading
            heading = QLabel(f"<b>v{version}</b> &nbsp; {title}")
            heading.setStyleSheet("font-size: 13px; color: #1a5276;")
            layout.addWidget(heading)

            # Bullet points
            for bullet in bullets:
                row = QLabel(f"  •  {bullet}")
                row.setWordWrap(True)
                row.setStyleSheet("color: #333; font-size: 11px;")
                layout.addWidget(row)

            # Divider between versions
            line = QFrame()
            line.setFrameShape(QFrame.Shape.HLine)
            line.setStyleSheet("color: #e0e0e0;")
            layout.addWidget(line)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Close)
        buttons.rejected.connect(self.accept)
        outer.addWidget(buttons)
