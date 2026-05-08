from PyQt6.QtWidgets import QWidget, QHBoxLayout, QPushButton, QLineEdit, QLabel
from PyQt6.QtCore import pyqtSignal, QTimer
from PyQt6.QtGui import QIcon


class Toolbar(QWidget):
    add_clicked = pyqtSignal()
    edit_clicked = pyqtSignal()
    delete_clicked = pyqtSignal()
    backup_clicked = pyqtSignal()
    print_clicked = pyqtSignal()
    search_changed = pyqtSignal(str)

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 6, 8, 6)
        layout.setSpacing(6)

        self._btn_add = QPushButton("+ Add Asset")
        self._btn_add.setFixedHeight(32)
        self._btn_add.clicked.connect(self.add_clicked)

        self._btn_edit = QPushButton("Edit Asset")
        self._btn_edit.setFixedHeight(32)
        self._btn_edit.setEnabled(False)
        self._btn_edit.clicked.connect(self.edit_clicked)

        self._btn_delete = QPushButton("Delete Asset")
        self._btn_delete.setFixedHeight(32)
        self._btn_delete.setEnabled(False)
        self._btn_delete.clicked.connect(self.delete_clicked)

        self._btn_backup = QPushButton("Backup")
        self._btn_backup.setFixedHeight(32)
        self._btn_backup.clicked.connect(self.backup_clicked)

        self._btn_print = QPushButton("Print Report")
        self._btn_print.setFixedHeight(32)
        self._btn_print.clicked.connect(self.print_clicked)

        self._search = QLineEdit()
        self._search.setPlaceholderText("Search by name...")
        self._search.setClearButtonEnabled(True)
        self._search.setFixedHeight(32)
        self._search.setMaximumWidth(280)

        self._debounce = QTimer(self)
        self._debounce.setSingleShot(True)
        self._debounce.setInterval(300)
        self._debounce.timeout.connect(self._emit_search)
        self._search.textChanged.connect(self._debounce.start)

        layout.addWidget(self._btn_add)
        layout.addWidget(self._btn_edit)
        layout.addWidget(self._btn_delete)
        layout.addWidget(self._btn_backup)
        layout.addWidget(self._btn_print)
        layout.addStretch()
        layout.addWidget(QLabel("Search:"))
        layout.addWidget(self._search)

    def set_asset_actions_enabled(self, enabled: bool) -> None:
        self._btn_edit.setEnabled(enabled)
        self._btn_delete.setEnabled(enabled)

    def set_delete_enabled(self, enabled: bool) -> None:
        self._btn_delete.setEnabled(enabled)

    def _emit_search(self) -> None:
        self.search_changed.emit(self._search.text().strip())
