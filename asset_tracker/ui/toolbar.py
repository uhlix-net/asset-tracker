from PyQt6.QtWidgets import (
    QWidget, QHBoxLayout, QPushButton, QLineEdit, QLabel, QComboBox,
    QApplication, QStyle,
)
from PyQt6.QtCore import pyqtSignal, QTimer, QSize
from ..config import ASSET_CATEGORIES


def _icon(sp: QStyle.StandardPixmap):
    """Return a standard system icon for the given StandardPixmap."""
    return QApplication.style().standardIcon(sp)


class Toolbar(QWidget):
    add_clicked = pyqtSignal()
    edit_clicked = pyqtSignal()
    delete_clicked = pyqtSignal()
    duplicate_clicked = pyqtSignal()
    backup_clicked = pyqtSignal()
    print_clicked = pyqtSignal()
    search_changed = pyqtSignal(str)
    category_changed = pyqtSignal(str)

    _ICON_SIZE = QSize(16, 16)

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 6, 8, 6)
        layout.setSpacing(6)

        SP = QStyle.StandardPixmap

        self._btn_add = QPushButton("Add Asset")
        self._btn_add.setIcon(_icon(SP.SP_FileDialogNewFolder))
        self._btn_add.setIconSize(self._ICON_SIZE)
        self._btn_add.setFixedHeight(32)
        self._btn_add.clicked.connect(self.add_clicked)

        self._btn_edit = QPushButton("Edit")
        self._btn_edit.setIcon(_icon(SP.SP_FileDialogDetailedView))
        self._btn_edit.setIconSize(self._ICON_SIZE)
        self._btn_edit.setFixedHeight(32)
        self._btn_edit.setEnabled(False)
        self._btn_edit.clicked.connect(self.edit_clicked)

        self._btn_duplicate = QPushButton("Duplicate")
        self._btn_duplicate.setIcon(_icon(SP.SP_FileLinkIcon))
        self._btn_duplicate.setIconSize(self._ICON_SIZE)
        self._btn_duplicate.setFixedHeight(32)
        self._btn_duplicate.setEnabled(False)
        self._btn_duplicate.clicked.connect(self.duplicate_clicked)

        self._btn_delete = QPushButton("Delete")
        self._btn_delete.setIcon(_icon(SP.SP_TrashIcon))
        self._btn_delete.setIconSize(self._ICON_SIZE)
        self._btn_delete.setFixedHeight(32)
        self._btn_delete.setEnabled(False)
        self._btn_delete.clicked.connect(self.delete_clicked)

        self._btn_backup = QPushButton("Backup")
        self._btn_backup.setIcon(_icon(SP.SP_DialogSaveButton))
        self._btn_backup.setIconSize(self._ICON_SIZE)
        self._btn_backup.setFixedHeight(32)
        self._btn_backup.clicked.connect(self.backup_clicked)

        self._btn_print = QPushButton("Print Report")
        self._btn_print.setIcon(_icon(SP.SP_FileIcon))
        self._btn_print.setIconSize(self._ICON_SIZE)
        self._btn_print.setFixedHeight(32)
        self._btn_print.clicked.connect(self.print_clicked)

        self._category = QComboBox()
        self._category.setFixedHeight(32)
        self._category.setMinimumWidth(140)
        self._category.addItem("All Categories", "")
        for cat in ASSET_CATEGORIES:
            self._category.addItem(cat, cat)
        self._category.currentIndexChanged.connect(
            lambda: self.category_changed.emit(self._category.currentData())
        )

        self._search = QLineEdit()
        self._search.setPlaceholderText("Search by name...")
        self._search.setClearButtonEnabled(True)
        self._search.setFixedHeight(32)
        self._search.setMaximumWidth(240)

        self._debounce = QTimer(self)
        self._debounce.setSingleShot(True)
        self._debounce.setInterval(300)
        self._debounce.timeout.connect(
            lambda: self.search_changed.emit(self._search.text().strip())
        )
        self._search.textChanged.connect(self._debounce.start)

        layout.addWidget(self._btn_add)
        layout.addWidget(self._btn_edit)
        layout.addWidget(self._btn_duplicate)
        layout.addWidget(self._btn_delete)
        layout.addWidget(self._btn_backup)
        layout.addWidget(self._btn_print)
        layout.addStretch()
        layout.addWidget(self._category)
        layout.addWidget(QLabel("Search:"))
        layout.addWidget(self._search)

    def set_asset_actions_enabled(self, enabled: bool) -> None:
        self._btn_edit.setEnabled(enabled)
        self._btn_duplicate.setEnabled(enabled)
        self._btn_delete.setEnabled(enabled)

    def set_delete_enabled(self, enabled: bool) -> None:
        self._btn_delete.setEnabled(enabled)
