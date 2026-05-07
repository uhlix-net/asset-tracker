from __future__ import annotations

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QSplitter, QMessageBox, QFileDialog,
    QInputDialog, QLineEdit,
)
from PyQt6.QtCore import Qt, QSettings

from ..database import Database
from ..config import APP_NAME, APP_VERSION
from .. import storage
from .toolbar import Toolbar
from .asset_list import AssetList
from .preview_panel import PreviewPanel


class MainWindow(QMainWindow):
    def __init__(self, db: Database) -> None:
        super().__init__()
        self._db = db
        self._settings = QSettings(APP_NAME, APP_NAME)

        self.setWindowTitle(f"{APP_NAME} v{APP_VERSION}")
        self.resize(1100, 680)

        # Restore geometry
        if self._settings.contains("geometry"):
            self.restoreGeometry(self._settings.value("geometry"))

        self._build_ui()
        self._refresh_assets()

    def _build_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        root_layout = QVBoxLayout(central)
        root_layout.setContentsMargins(0, 0, 0, 0)
        root_layout.setSpacing(0)

        self._toolbar = Toolbar()
        root_layout.addWidget(self._toolbar)

        # Horizontal divider line
        line = QWidget()
        line.setFixedHeight(1)
        line.setStyleSheet("background: #d0d0d0;")
        root_layout.addWidget(line)

        self._splitter = QSplitter(Qt.Orientation.Horizontal)
        root_layout.addWidget(self._splitter)

        self._asset_list = AssetList()
        self._preview = PreviewPanel(self._db)

        self._splitter.addWidget(self._asset_list)
        self._splitter.addWidget(self._preview)
        self._splitter.setStretchFactor(0, 3)
        self._splitter.setStretchFactor(1, 2)

        if self._settings.contains("splitter"):
            self._splitter.restoreState(self._settings.value("splitter"))

        # Signals
        self._toolbar.add_clicked.connect(self._on_add)
        self._toolbar.delete_clicked.connect(self._on_delete)
        self._toolbar.backup_clicked.connect(self._on_backup)
        self._toolbar.print_clicked.connect(self._on_print)
        self._toolbar.search_changed.connect(self._on_search)

        self._asset_list.asset_selected.connect(self._on_asset_selected)
        self._asset_list.selection_cleared.connect(self._on_selection_cleared)

    def _refresh_assets(self, search: str = "") -> None:
        assets = self._db.get_all_assets(search=search)
        self._asset_list.load_assets(assets)

    def _on_asset_selected(self, asset) -> None:
        self._toolbar.set_delete_enabled(True)
        self._preview.show_asset(asset)

    def _on_selection_cleared(self) -> None:
        self._toolbar.set_delete_enabled(False)
        self._preview.clear()

    def _on_search(self, text: str) -> None:
        self._refresh_assets(search=text)

    def _on_add(self) -> None:
        from .asset_form import AssetFormDialog
        dlg = AssetFormDialog(self._db, self)
        if dlg.exec():
            self._refresh_assets()

    def _on_delete(self) -> None:
        asset = self._asset_list.get_selected_asset()
        if not asset:
            return
        reply = QMessageBox.question(
            self,
            "Delete Asset",
            f"Permanently delete asset '{asset.name}' (ID: {asset.id})?\n\n"
            "This will remove all associated files and cannot be undone.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if reply == QMessageBox.StandardButton.Yes:
            storage.delete_asset_files(asset)
            self._db.delete_asset(asset.id)
            self._preview.clear()
            self._toolbar.set_delete_enabled(False)
            self._refresh_assets()

    def _on_backup(self) -> None:
        from .. import backup
        password, ok = QInputDialog.getText(
            self, "Backup Password", "Enter a password for the backup:",
            QLineEdit.EchoMode.Password,
        )
        if not ok or not password:
            return
        confirm, ok2 = QInputDialog.getText(
            self, "Confirm Password", "Re-enter the password:",
            QLineEdit.EchoMode.Password,
        )
        if not ok2 or password != confirm:
            QMessageBox.warning(self, "Backup", "Passwords do not match. Backup cancelled.")
            return

        dest, _ = QFileDialog.getSaveFileName(
            self, "Save Backup", backup.default_backup_name(), "Zip Files (*.zip)"
        )
        if not dest:
            return

        try:
            backup.create_backup(password, dest)
            QMessageBox.information(self, "Backup", f"Backup saved to:\n{dest}")
        except Exception as e:
            QMessageBox.critical(self, "Backup Failed", str(e))

    def _on_print(self) -> None:
        from .. import report
        import os

        dest, _ = QFileDialog.getSaveFileName(
            self, "Save PDF Report", "Asset_Inventory_Report.pdf", "PDF Files (*.pdf)"
        )
        if not dest:
            return

        try:
            data = self._db.get_all_assets_with_files()
            report.generate_report(data, dest)
            reply = QMessageBox.information(
                self,
                "Report Generated",
                f"Report saved to:\n{dest}\n\nOpen the file now?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            )
            if reply == QMessageBox.StandardButton.Yes:
                if os.name == "nt":
                    os.startfile(dest)
                else:
                    import subprocess
                    subprocess.Popen(["xdg-open", dest])
        except Exception as e:
            QMessageBox.critical(self, "Report Failed", str(e))

    def closeEvent(self, event) -> None:
        self._settings.setValue("geometry", self.saveGeometry())
        self._settings.setValue("splitter", self._splitter.saveState())
        super().closeEvent(event)
