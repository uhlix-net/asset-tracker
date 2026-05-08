from __future__ import annotations
import sqlite3
from datetime import datetime, timezone

from .config import DB_DIR, DB_PATH
from .models import Asset, AssetFile


def _now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


class Database:
    def __init__(self) -> None:
        DB_DIR.mkdir(parents=True, exist_ok=True)
        self._conn = sqlite3.connect(str(DB_PATH), check_same_thread=False)
        self._conn.row_factory = sqlite3.Row
        self._conn.execute("PRAGMA foreign_keys = ON")
        self._create_schema()
        self._migrate()

    def _create_schema(self) -> None:
        self._conn.executescript("""
            CREATE TABLE IF NOT EXISTS assets (
                id              TEXT PRIMARY KEY,
                name            TEXT NOT NULL,
                category        TEXT NOT NULL DEFAULT '',
                date_purchase   TEXT,
                value_estimate  REAL,
                current_value   REAL,
                serial_number   TEXT NOT NULL DEFAULT '',
                model_number    TEXT NOT NULL DEFAULT '',
                has_receipt     INTEGER NOT NULL DEFAULT 0,
                date_added      TEXT NOT NULL,
                notes           TEXT NOT NULL DEFAULT ''
            );

            CREATE TABLE IF NOT EXISTS asset_files (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                asset_id    TEXT NOT NULL REFERENCES assets(id) ON DELETE CASCADE,
                file_name   TEXT NOT NULL,
                file_type   TEXT NOT NULL,
                stored_name TEXT NOT NULL,
                date_added  TEXT NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_af_asset_id ON asset_files(asset_id);
        """)
        self._conn.commit()

    def _migrate(self) -> None:
        """Add columns introduced after initial release to existing databases."""
        existing = {
            row[1] for row in self._conn.execute("PRAGMA table_info(assets)")
        }
        migrations = {
            "category":      "ALTER TABLE assets ADD COLUMN category TEXT NOT NULL DEFAULT ''",
            "current_value": "ALTER TABLE assets ADD COLUMN current_value REAL",
            "serial_number": "ALTER TABLE assets ADD COLUMN serial_number TEXT NOT NULL DEFAULT ''",
            "model_number":  "ALTER TABLE assets ADD COLUMN model_number TEXT NOT NULL DEFAULT ''",
        }
        for col, sql in migrations.items():
            if col not in existing:
                self._conn.execute(sql)
        self._conn.commit()

    def next_asset_id(self) -> str:
        row = self._conn.execute(
            "SELECT MAX(CAST(id AS INTEGER)) AS max_id FROM assets"
        ).fetchone()
        next_int = (row["max_id"] or 0) + 1
        return f"{next_int:05d}"

    def insert_asset(self, asset: Asset) -> None:
        self._conn.execute(
            """INSERT INTO assets (id, name, category, date_purchase, value_estimate,
               current_value, serial_number, model_number, has_receipt, date_added, notes)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                asset.id, asset.name, asset.category, asset.date_purchase,
                asset.value_estimate, asset.current_value,
                asset.serial_number, asset.model_number,
                1 if asset.has_receipt else 0,
                asset.date_added or _now(), asset.notes,
            ),
        )
        self._conn.commit()

    def insert_asset_file(self, f: AssetFile) -> None:
        self._conn.execute(
            """INSERT INTO asset_files (asset_id, file_name, file_type, stored_name, date_added)
               VALUES (?, ?, ?, ?, ?)""",
            (f.asset_id, f.file_name, f.file_type, f.stored_name, f.date_added or _now()),
        )
        self._conn.commit()

    def get_all_assets(
        self,
        search: str = "",
        category: str = "",
        min_value: float | None = None,
        max_value: float | None = None,
    ) -> list[Asset]:
        clauses, params = [], []
        if search:
            clauses.append("name LIKE ?")
            params.append(f"%{search}%")
        if category:
            clauses.append("category = ?")
            params.append(category)
        if min_value is not None:
            clauses.append("value_estimate >= ?")
            params.append(min_value)
        if max_value is not None:
            clauses.append("value_estimate <= ?")
            params.append(max_value)
        where = f"WHERE {' AND '.join(clauses)}" if clauses else ""
        rows = self._conn.execute(
            f"SELECT * FROM assets {where} ORDER BY category, name",
            params,
        ).fetchall()
        return [self._row_to_asset(r) for r in rows]

    def get_asset_by_id(self, asset_id: str) -> Asset | None:
        row = self._conn.execute(
            "SELECT * FROM assets WHERE id = ?", (asset_id,)
        ).fetchone()
        return self._row_to_asset(row) if row else None

    def get_asset_files(self, asset_id: str) -> list[AssetFile]:
        rows = self._conn.execute(
            "SELECT * FROM asset_files WHERE asset_id = ? ORDER BY file_type, id",
            (asset_id,),
        ).fetchall()
        return [self._row_to_file(r) for r in rows]

    def get_all_assets_with_files(self) -> list[tuple[Asset, list[AssetFile]]]:
        assets = self.get_all_assets()
        return [(a, self.get_asset_files(a.id)) for a in assets]

    def get_totals(self) -> dict:
        row = self._conn.execute(
            """SELECT COUNT(*) as count,
                      SUM(value_estimate) as purchase_total,
                      SUM(current_value) as current_total
               FROM assets"""
        ).fetchone()
        return {
            "count": row["count"] or 0,
            "purchase_total": row["purchase_total"],
            "current_total": row["current_total"],
        }

    def update_asset(self, asset: Asset) -> None:
        self._conn.execute(
            """UPDATE assets SET name=?, category=?, date_purchase=?, value_estimate=?,
               current_value=?, serial_number=?, model_number=?,
               has_receipt=?, notes=? WHERE id=?""",
            (
                asset.name, asset.category, asset.date_purchase,
                asset.value_estimate, asset.current_value,
                asset.serial_number, asset.model_number,
                1 if asset.has_receipt else 0,
                asset.notes, asset.id,
            ),
        )
        self._conn.commit()

    def update_notes(self, asset_id: str, notes: str) -> None:
        self._conn.execute(
            "UPDATE assets SET notes = ? WHERE id = ?", (notes, asset_id)
        )
        self._conn.commit()

    def delete_asset(self, asset_id: str) -> None:
        self._conn.execute("DELETE FROM assets WHERE id = ?", (asset_id,))
        self._conn.commit()

    def delete_asset_file(self, file_id: int) -> None:
        self._conn.execute("DELETE FROM asset_files WHERE id = ?", (file_id,))
        self._conn.commit()

    def close(self) -> None:
        self._conn.close()

    @staticmethod
    def _row_to_asset(row: sqlite3.Row) -> Asset:
        return Asset(
            id=row["id"],
            name=row["name"],
            category=row["category"] or "",
            date_purchase=row["date_purchase"],
            value_estimate=row["value_estimate"],
            current_value=row["current_value"],
            serial_number=row["serial_number"] or "",
            model_number=row["model_number"] or "",
            has_receipt=bool(row["has_receipt"]),
            date_added=row["date_added"],
            notes=row["notes"] or "",
        )

    @staticmethod
    def _row_to_file(row: sqlite3.Row) -> AssetFile:
        return AssetFile(
            id=row["id"],
            asset_id=row["asset_id"],
            file_name=row["file_name"],
            file_type=row["file_type"],
            stored_name=row["stored_name"],
            date_added=row["date_added"],
        )
