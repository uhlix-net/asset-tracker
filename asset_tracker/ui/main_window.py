from __future__ import annotations
import os

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QSplitter, QMessageBox, QFileDialog,
    QInputDialog, QLineEdit, QStatusBar,
)
from PyQt6.QtCore import Qt, QSettings, QTimer
from PyQt6.QtGui import QAction

from ..database import Database
from ..config import APP_NAME, APP_VERSION
from .. import storage, backup, export
from .toolbar import Toolbar
from .asset_list import AssetList
from .preview_panel import PreviewPanel


class MainWindow(QMainWindow):
    def __init__(self, db: Database) -> None:
        super().__init__()
        self._db = db
        self._settings = QSettings(APP_NAME, APP_NAME)
        self._search = ""
        self._category = ""

        self.setWindowTitle(f"{APP_NAME} v{APP_VERSION}")
        self.resize(1200, 720)

        if self._settings.contains("geometry"):
            self.restoreGeometry(self._settings.value("geometry"))

        self._build_menu()
        self._build_ui()
        self._build_status_bar()
        self._refresh_assets()

    # ── Menu bar ─────────────────────────────────────────────────────────────

    def _build_menu(self) -> None:
        menubar = self.menuBar()

        # File menu
        file_menu = menubar.addMenu("File")

        act_csv = QAction("Export to CSV...", self)
        act_csv.triggered.connect(self._on_export_csv)
        file_menu.addAction(act_csv)

        file_menu.addSeparator()

        act_backup = QAction("Create Backup...", self)
        act_backup.triggered.connect(self._on_backup)
        file_menu.addAction(act_backup)

        act_restore = QAction("Restore from Backup...", self)
        act_restore.triggered.connect(self._on_restore)
        file_menu.addAction(act_restore)

        file_menu.addSeparator()

        self._act_auto_backup = QAction("Auto-backup on Exit", self)
        self._act_auto_backup.setCheckable(True)
        self._act_auto_backup.setChecked(
            self._settings.value("auto_backup_on_exit", False, type=bool)
        )
        self._act_auto_backup.toggled.connect(
            lambda checked: self._settings.setValue("auto_backup_on_exit", checked)
        )
        file_menu.addAction(self._act_auto_backup)

        file_menu.addSeparator()

        act_exit = QAction("Exit", self)
        act_exit.triggered.connect(self.close)
        file_menu.addAction(act_exit)

        # Help menu
        help_menu = menubar.addMenu("Help")

        act_about = QAction(f"About {APP_NAME}", self)
        act_about.triggered.connect(self._on_about)
        help_menu.addAction(act_about)

        act_history = QAction("Update History", self)
        act_history.triggered.connect(self._on_update_history)
        help_menu.addAction(act_history)

    # ── Central UI ───────────────────────────────────────────────────────────

    def _build_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        # Toolbar — anchored at top, full window width
        self._toolbar = Toolbar()
        root.addWidget(self._toolbar)

        # Divider line — always spans the full window width
        line = QWidget()
        line.setFixedHeight(1)
        line.setStyleSheet("background: #d0d0d0;")
        root.addWidget(line)

        # Splitter — 50/50 horizontal split, anchored to top
        self._splitter = QSplitter(Qt.Orientation.Horizontal)

        self._asset_list = AssetList(self._db)
        self._preview = PreviewPanel(self._db)
        self._splitter.addWidget(self._asset_list)
        self._splitter.addWidget(self._preview)

        # Equal stretch factors give each panel exactly 50% of window width
        self._splitter.setStretchFactor(0, 1)
        self._splitter.setStretchFactor(1, 1)

        if self._settings.contains("splitter"):
            self._splitter.restoreState(self._settings.value("splitter"))
        else:
            half = self.width() // 2
            self._splitter.setSizes([half, half])

        root.addWidget(self._splitter)
        # Absorbs extra vertical space so all content stays at the top
        root.addStretch(1)

        # Toolbar signals
        self._toolbar.add_clicked.connect(self._on_add)
        self._toolbar.edit_clicked.connect(self._on_edit)
        self._toolbar.delete_clicked.connect(self._on_delete_selected)
        self._toolbar.duplicate_clicked.connect(self._on_duplicate)
        self._toolbar.backup_clicked.connect(self._on_backup)
        self._toolbar.print_clicked.connect(self._on_print)
        self._toolbar.search_changed.connect(self._on_search)
        self._toolbar.category_changed.connect(self._on_category)

        # Asset list signals
        self._asset_list.asset_selected.connect(self._on_asset_selected)
        self._asset_list.selection_cleared.connect(self._on_selection_cleared)
        self._asset_list.edit_requested.connect(self._on_edit)
        self._asset_list.duplicate_requested.connect(self._on_duplicate)
        self._asset_list.delete_requested.connect(self._on_delete_assets)

    def _build_status_bar(self) -> None:
        from PyQt6.QtWidgets import QLabel
        sb = QStatusBar()
        self.setStatusBar(sb)
        self._lbl_count = QLabel()
        self._lbl_values = QLabel()
        sb.addWidget(self._lbl_count)
        sb.addPermanentWidget(self._lbl_values)

    # ── Refresh ───────────────────────────────────────────────────────────────

    def _refresh_assets(self) -> None:
        assets = self._db.get_all_assets(search=self._search, category=self._category)
        self._asset_list.load_assets(assets)
        self._update_status_bar()

    def _update_status_bar(self) -> None:
        totals = self._db.get_totals()
        self._lbl_count.setText(f"  {totals['count']} asset{'s' if totals['count'] != 1 else ''}")
        parts = []
        if totals["purchase_total"] is not None:
            parts.append(f"Purchase Price: ${totals['purchase_total']:,.2f}")
        if totals["current_total"] is not None:
            parts.append(f"Current: ${totals['current_total']:,.2f}")
        self._lbl_values.setText("  |  ".join(parts) + "  " if parts else "")

    # ── Selection ─────────────────────────────────────────────────────────────

    def _on_asset_selected(self, asset) -> None:
        self._toolbar.set_asset_actions_enabled(True)
        self._preview.show_asset(asset)

    def _on_selection_cleared(self) -> None:
        self._toolbar.set_asset_actions_enabled(False)
        self._preview.clear()

    # ── Filter / search ───────────────────────────────────────────────────────

    def _on_search(self, text: str) -> None:
        self._search = text
        self._refresh_assets()

    def _on_category(self, category: str) -> None:
        self._category = category
        self._refresh_assets()

    # ── CRUD ─────────────────────────────────────────────────────────────────

    def _on_add(self) -> None:
        from .asset_form import AssetFormDialog
        if AssetFormDialog(self._db, self).exec():
            self._refresh_assets()

    def _on_edit(self, asset=None) -> None:
        asset = asset or self._asset_list.get_selected_asset()
        if not asset:
            return
        from .edit_asset_form import EditAssetFormDialog
        if EditAssetFormDialog(self._db, asset, self).exec():
            self._refresh_assets()
            updated = self._db.get_asset_by_id(asset.id)
            if updated:
                self._preview.show_asset(updated)

    def _on_duplicate(self, asset=None) -> None:
        asset = asset or self._asset_list.get_selected_asset()
        if not asset:
            return
        from datetime import datetime, timezone
        from ..models import Asset
        new_id = self._db.next_asset_id()
        copy = Asset(
            id=new_id,
            name=f"{asset.name} (Copy)",
            category=asset.category,
            date_purchase=asset.date_purchase,
            value_estimate=asset.value_estimate,
            current_value=asset.current_value,
            serial_number=asset.serial_number,
            model_number=asset.model_number,
            has_receipt=False,
            date_added=datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            notes=asset.notes,
        )
        self._db.insert_asset(copy)
        self._refresh_assets()

    def _on_delete_selected(self) -> None:
        assets = self._asset_list.get_selected_assets()
        if assets:
            self._on_delete_assets(assets)

    def _on_delete_assets(self, assets: list) -> None:
        if not assets:
            return
        names = "\n".join(f"  • {a.name} ({a.id})" for a in assets[:10])
        if len(assets) > 10:
            names += f"\n  … and {len(assets) - 10} more"
        reply = QMessageBox.question(
            self, "Delete Assets",
            f"Permanently delete {len(assets)} asset{'s' if len(assets) > 1 else ''}?\n\n"
            f"{names}\n\nThis removes all files and cannot be undone.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if reply == QMessageBox.StandardButton.Yes:
            for asset in assets:
                storage.delete_asset_files(asset)
                self._db.delete_asset(asset.id)
            self._preview.clear()
            self._toolbar.set_asset_actions_enabled(False)
            self._refresh_assets()

    # ── File menu actions ─────────────────────────────────────────────────────

    def _on_backup(self) -> None:
        password, ok = QInputDialog.getText(
            self, "Backup Password", "Enter a password for the backup:",
            QLineEdit.EchoMode.Password)
        if not ok or not password:
            return
        confirm, ok2 = QInputDialog.getText(
            self, "Confirm Password", "Re-enter the password:",
            QLineEdit.EchoMode.Password)
        if not ok2 or password != confirm:
            QMessageBox.warning(self, "Backup", "Passwords do not match.")
            return
        dest, _ = QFileDialog.getSaveFileName(
            self, "Save Backup", backup.default_backup_name(), "Zip Files (*.zip)")
        if not dest:
            return
        try:
            backup.create_backup(password, dest)
            self._settings.setValue("last_backup_path",
                                    str(__import__("pathlib").Path(dest).parent))
            QMessageBox.information(self, "Backup Complete", f"Backup saved to:\n{dest}")
        except Exception as e:
            QMessageBox.critical(self, "Backup Failed", str(e))

    def _on_restore(self) -> None:
        QMessageBox.warning(
            self, "Restore from Backup",
            "Restoring will replace all current data with the backup contents.\n"
            "Your existing data will be moved to a timestamped folder first.",
        )
        src, _ = QFileDialog.getOpenFileName(
            self, "Select Backup File", "", "Zip Files (*.zip)")
        if not src:
            return
        password, ok = QInputDialog.getText(
            self, "Backup Password", "Enter the backup password:",
            QLineEdit.EchoMode.Password)
        if not ok:
            return
        try:
            backup.restore_backup(password, src)
            QMessageBox.information(
                self, "Restore Complete",
                "Backup restored successfully.\nPlease restart the application.")
        except Exception as e:
            QMessageBox.critical(self, "Restore Failed",
                                 f"Could not restore backup:\n{e}")

    def _on_export_csv(self) -> None:
        dest, _ = QFileDialog.getSaveFileName(
            self, "Export to CSV", export.default_export_name(), "CSV Files (*.csv)")
        if not dest:
            return
        try:
            assets = self._db.get_all_assets()
            export.export_csv(assets, dest)
            reply = QMessageBox.information(
                self, "Export Complete", f"Exported {len(assets)} assets to:\n{dest}\n\nOpen file?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            )
            if reply == QMessageBox.StandardButton.Yes:
                if os.name == "nt":
                    os.startfile(dest)
                else:
                    import subprocess
                    subprocess.Popen(["xdg-open", dest])
        except Exception as e:
            QMessageBox.critical(self, "Export Failed", str(e))

    def _on_print(self) -> None:
        from .. import report
        dest, _ = QFileDialog.getSaveFileName(
            self, "Save PDF Report", "Asset_Inventory_Report.pdf", "PDF Files (*.pdf)")
        if not dest:
            return
        try:
            data = self._db.get_all_assets_with_files()
            report.generate_report(data, dest)
            reply = QMessageBox.information(
                self, "Report Generated", f"Report saved to:\n{dest}\n\nOpen file?",
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

    def _on_about(self) -> None:
        from .about_dialog import AboutDialog
        AboutDialog(self).exec()

    def _on_update_history(self) -> None:
        from .update_history_dialog import UpdateHistoryDialog
        UpdateHistoryDialog(self).exec()

    # ── Close / persist ───────────────────────────────────────────────────────

    def showEvent(self, event) -> None:
        super().showEvent(event)
        if not hasattr(self, "_height_locked"):
            self._height_locked = True
            # Run after Qt finishes its first layout pass so height() is correct
            QTimer.singleShot(0, self._lock_splitter_height)

    def _lock_splitter_height(self) -> None:
        h = self._splitter.height()
        if h > 0:
            self._splitter.setFixedHeight(h)

    def closeEvent(self, event) -> None:
        if self._settings.value("auto_backup_on_exit", False, type=bool):
            reply = QMessageBox.question(
                self, "Auto-backup",
                "Create a backup before exiting?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
                | QMessageBox.StandardButton.Cancel,
                QMessageBox.StandardButton.Yes,
            )
            if reply == QMessageBox.StandardButton.Cancel:
                event.ignore()
                return
            if reply == QMessageBox.StandardButton.Yes:
                self._on_backup()

        self._settings.setValue("geometry", self.saveGeometry())
        self._settings.setValue("splitter", self._splitter.saveState())
        super().closeEvent(event)
