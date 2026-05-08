from __future__ import annotations

from PyQt6.QtWidgets import (
    QTableWidget, QTableWidgetItem, QHeaderView, QAbstractItemView, QMenu,
    QLabel,
)
from PyQt6.QtCore import pyqtSignal, Qt
from PyQt6.QtGui import QAction

from ..models import Asset


class AssetList(QTableWidget):
    asset_selected = pyqtSignal(object)
    selection_cleared = pyqtSignal()
    edit_requested = pyqtSignal(object)
    duplicate_requested = pyqtSignal(object)
    delete_requested = pyqtSignal(list)

    # Column 0 is the thumbnail; the rest follow
    _COLUMNS = ["", "ID", "Name", "Category", "Purchase Date",
                "Purchase Price", "Current Value", "Receipt?", "Date Added"]
    _THUMB_COL = 0
    _RIGHT_ALIGN = {5, 6}   # Purchase Price, Current Value

    def __init__(self, db=None, parent=None) -> None:
        super().__init__(0, len(self._COLUMNS), parent)
        self._db = db
        self._assets: list[Asset] = []

        self.setHorizontalHeaderLabels(self._COLUMNS)
        self.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        self.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.setSortingEnabled(True)
        self.setAlternatingRowColors(True)
        self.verticalHeader().setVisible(False)
        self.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.customContextMenuRequested.connect(self._show_context_menu)

        # Row height to accommodate thumbnails
        self.verticalHeader().setDefaultSectionSize(52)

        # Column widths
        hh = self.horizontalHeader()
        hh.setStretchLastSection(False)
        hh.setSectionResizeMode(0, QHeaderView.ResizeMode.Fixed)          # thumb
        self.setColumnWidth(0, 52)
        hh.setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)  # ID
        hh.setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)           # Name
        hh.setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)  # Category
        hh.setSectionResizeMode(4, QHeaderView.ResizeMode.ResizeToContents)  # Purchase Date
        hh.setSectionResizeMode(5, QHeaderView.ResizeMode.ResizeToContents)  # Purchase Price
        hh.setSectionResizeMode(6, QHeaderView.ResizeMode.ResizeToContents)  # Current Value
        hh.setSectionResizeMode(7, QHeaderView.ResizeMode.ResizeToContents)  # Receipt?
        hh.setSectionResizeMode(8, QHeaderView.ResizeMode.ResizeToContents)  # Date Added

        # Sort indicator — visible, default by Date Added descending
        hh.setSortIndicatorShown(True)
        hh.setSortIndicator(8, Qt.SortOrder.DescendingOrder)

        # Empty-state overlay
        self._empty_label = QLabel(
            "No assets yet — click  Add Asset  to get started"
        )
        self._empty_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._empty_label.setStyleSheet(
            "color: #888; font-size: 14px; font-style: italic;"
        )
        self._empty_label.setParent(self.viewport())
        self._empty_label.hide()

        self.itemSelectionChanged.connect(self._on_selection_changed)

    # ── Resize ────────────────────────────────────────────────────────────────

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._empty_label.resize(self.viewport().size())

    # ── Load ──────────────────────────────────────────────────────────────────

    def load_assets(self, assets: list[Asset]) -> None:
        self._assets = assets
        self.setSortingEnabled(False)
        self.setRowCount(0)
        for asset in assets:
            row = self.rowCount()
            self.insertRow(row)
            self._populate_row(row, asset)
        self.setSortingEnabled(True)

        if assets:
            self._empty_label.hide()
        else:
            self._empty_label.resize(self.viewport().size())
            self._empty_label.show()
            self._empty_label.raise_()

    def _populate_row(self, row: int, asset: Asset) -> None:
        # ── Thumbnail (col 0) ────────────────────────────────────────────────
        thumb_item = QTableWidgetItem()
        thumb_item.setData(Qt.ItemDataRole.UserRole, asset.id)
        self.setItem(row, 0, thumb_item)

        pixmap = self._load_thumbnail(asset)
        lbl = QLabel()
        lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        if pixmap:
            lbl.setPixmap(
                pixmap.scaled(40, 40,
                              Qt.AspectRatioMode.KeepAspectRatio,
                              Qt.TransformationMode.SmoothTransformation)
            )
        else:
            lbl.setText("📷")
            lbl.setStyleSheet("color: #bbb; font-size: 20px;")
        self.setCellWidget(row, 0, lbl)

        # ── Text columns (cols 1-8) ──────────────────────────────────────────
        values = [
            asset.id,
            asset.name,
            asset.category or "—",
            asset.date_purchase or "—",
            asset.value_display,
            asset.current_value_display,
            "Yes" if asset.has_receipt else "No",
            asset.date_added[:10],
        ]
        for offset, text in enumerate(values):
            col = offset + 1
            item = QTableWidgetItem(text)
            item.setData(Qt.ItemDataRole.UserRole, asset.id)
            if col in self._RIGHT_ALIGN:
                item.setTextAlignment(
                    Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter
                )
            self.setItem(row, col, item)

    def _load_thumbnail(self, asset: Asset):
        if self._db is None:
            return None
        try:
            files = self._db.get_asset_files(asset.id)
            images = [f for f in files if f.file_type == "image"]
            if not images:
                return None
            from .. import storage
            first = images[0]
            path = storage.get_stored_path(asset, first)
            return storage.generate_thumbnail(path, size=(40, 40), encrypted=first.encrypted)
        except Exception:
            return None

    # ── Selection ─────────────────────────────────────────────────────────────

    def get_selected_asset(self) -> Asset | None:
        assets = self.get_selected_assets()
        return assets[0] if assets else None

    def get_selected_assets(self) -> list[Asset]:
        seen, result = set(), []
        for item in self.selectedItems():
            asset_id = item.data(Qt.ItemDataRole.UserRole)
            if asset_id and asset_id not in seen:
                seen.add(asset_id)
                asset = next((a for a in self._assets if a.id == asset_id), None)
                if asset:
                    result.append(asset)
        return result

    def _on_selection_changed(self) -> None:
        assets = self.get_selected_assets()
        if len(assets) == 1:
            self.asset_selected.emit(assets[0])
        elif not assets:
            self.selection_cleared.emit()

    # ── Context menu ──────────────────────────────────────────────────────────

    def _show_context_menu(self, pos) -> None:
        assets = self.get_selected_assets()
        if not assets:
            return
        menu = QMenu(self)
        if len(assets) == 1:
            menu.addAction("Edit Asset",
                           lambda: self.edit_requested.emit(assets[0]))
            menu.addAction("Duplicate Asset",
                           lambda: self.duplicate_requested.emit(assets[0]))
            menu.addSeparator()
        label = f"Delete {len(assets)} Asset{'s' if len(assets) > 1 else ''}"
        menu.addAction(label,
                       lambda: self.delete_requested.emit(assets))
        menu.exec(self.viewport().mapToGlobal(pos))
