from __future__ import annotations

from PyQt6.QtWidgets import (
    QTableWidget, QTableWidgetItem, QHeaderView, QAbstractItemView, QMenu,
)
from PyQt6.QtCore import pyqtSignal, Qt
from PyQt6.QtGui import QAction

from ..models import Asset


class AssetList(QTableWidget):
    asset_selected = pyqtSignal(object)   # emits Asset
    selection_cleared = pyqtSignal()
    edit_requested = pyqtSignal(object)
    duplicate_requested = pyqtSignal(object)
    delete_requested = pyqtSignal(list)   # emits list[Asset]

    _COLUMNS = ["ID", "Name", "Category", "Purchase Date",
                "Purchase Value", "Current Value", "Receipt?", "Date Added"]

    def __init__(self, parent=None) -> None:
        super().__init__(0, len(self._COLUMNS), parent)
        self.setHorizontalHeaderLabels(self._COLUMNS)
        self.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        self.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.setSortingEnabled(True)
        self.setAlternatingRowColors(True)
        self.verticalHeader().setVisible(False)
        self.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.customContextMenuRequested.connect(self._show_context_menu)

        hh = self.horizontalHeader()
        hh.setStretchLastSection(False)
        hh.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        hh.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        hh.setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        hh.setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        hh.setSectionResizeMode(4, QHeaderView.ResizeMode.ResizeToContents)
        hh.setSectionResizeMode(5, QHeaderView.ResizeMode.ResizeToContents)
        hh.setSectionResizeMode(6, QHeaderView.ResizeMode.ResizeToContents)
        hh.setSectionResizeMode(7, QHeaderView.ResizeMode.ResizeToContents)

        self.itemSelectionChanged.connect(self._on_selection_changed)
        self._assets: list[Asset] = []

    def load_assets(self, assets: list[Asset]) -> None:
        self._assets = assets
        self.setSortingEnabled(False)
        self.setRowCount(0)
        for asset in assets:
            row = self.rowCount()
            self.insertRow(row)
            self._populate_row(row, asset)
        self.setSortingEnabled(True)

    def _populate_row(self, row: int, asset: Asset) -> None:
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
        right_align = {4, 5}
        for col, text in enumerate(values):
            item = QTableWidgetItem(text)
            item.setData(Qt.ItemDataRole.UserRole, asset.id)
            if col in right_align:
                item.setTextAlignment(
                    Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter
                )
            self.setItem(row, col, item)

    def get_selected_asset(self) -> Asset | None:
        assets = self.get_selected_assets()
        return assets[0] if assets else None

    def get_selected_assets(self) -> list[Asset]:
        seen, result = set(), []
        for item in self.selectedItems():
            asset_id = item.data(Qt.ItemDataRole.UserRole)
            if asset_id not in seen:
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

    def _show_context_menu(self, pos) -> None:
        assets = self.get_selected_assets()
        if not assets:
            return
        menu = QMenu(self)
        if len(assets) == 1:
            menu.addAction("Edit Asset", lambda: self.edit_requested.emit(assets[0]))
            menu.addAction("Duplicate Asset", lambda: self.duplicate_requested.emit(assets[0]))
            menu.addSeparator()
        label = f"Delete {len(assets)} Asset{'s' if len(assets) > 1 else ''}"
        menu.addAction(label, lambda: self.delete_requested.emit(assets))
        menu.exec(self.viewport().mapToGlobal(pos))
