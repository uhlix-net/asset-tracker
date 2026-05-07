from __future__ import annotations
import pathlib

from PyQt6.QtWidgets import (
    QTableWidget, QTableWidgetItem, QHeaderView, QAbstractItemView, QLabel,
)
from PyQt6.QtCore import pyqtSignal, Qt
from PyQt6.QtGui import QPixmap

from ..models import Asset
from .. import storage


class AssetList(QTableWidget):
    asset_selected = pyqtSignal(object)  # emits Asset or None
    selection_cleared = pyqtSignal()

    _COLUMNS = ["ID", "Name", "Purchase Date", "Est. Value", "Receipt?", "Date Added"]
    _THUMB_COL = -1  # no thumb column in list view; thumbnails shown in preview

    def __init__(self, parent=None) -> None:
        super().__init__(0, len(self._COLUMNS), parent)
        self.setHorizontalHeaderLabels(self._COLUMNS)
        self.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.setSortingEnabled(True)
        self.setAlternatingRowColors(True)
        self.verticalHeader().setVisible(False)

        hh = self.horizontalHeader()
        hh.setStretchLastSection(False)
        hh.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        hh.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        hh.setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        hh.setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        hh.setSectionResizeMode(4, QHeaderView.ResizeMode.ResizeToContents)
        hh.setSectionResizeMode(5, QHeaderView.ResizeMode.ResizeToContents)

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
        items = [
            asset.id,
            asset.name,
            asset.date_purchase or "—",
            asset.value_display,
            "Yes" if asset.has_receipt else "No",
            asset.date_added[:10],
        ]
        for col, text in enumerate(items):
            item = QTableWidgetItem(text)
            item.setData(Qt.ItemDataRole.UserRole, asset.id)
            if col in (3,):  # right-align value
                item.setTextAlignment(
                    Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter
                )
            self.setItem(row, col, item)

    def get_selected_asset(self) -> Asset | None:
        rows = self.selectedItems()
        if not rows:
            return None
        asset_id = rows[0].data(Qt.ItemDataRole.UserRole)
        return next((a for a in self._assets if a.id == asset_id), None)

    def _on_selection_changed(self) -> None:
        asset = self.get_selected_asset()
        if asset:
            self.asset_selected.emit(asset)
        else:
            self.selection_cleared.emit()
