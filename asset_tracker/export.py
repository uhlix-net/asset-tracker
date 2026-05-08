from __future__ import annotations
import csv
import pathlib
from datetime import date

from .models import Asset


def default_export_name() -> str:
    return f"asset_inventory_{date.today().strftime('%Y-%m-%d')}.csv"


def export_csv(assets: list[Asset], dest: str | pathlib.Path) -> None:
    dest = pathlib.Path(dest)
    dest.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "Asset ID", "Name", "Category", "Serial Number", "Model Number",
        "Purchase Date", "Purchase Price", "Current Value",
        "Receipt on File", "Date Added", "Notes",
    ]

    with open(dest, "w", newline="", encoding="utf-8-sig") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for a in assets:
            writer.writerow({
                "Asset ID": a.id,
                "Name": a.name,
                "Category": a.category,
                "Serial Number": a.serial_number,
                "Model Number": a.model_number,
                "Purchase Date": a.date_purchase or "",
                "Purchase Price": f"{a.value_estimate:.2f}" if a.value_estimate is not None else "",
                "Current Value": f"{a.current_value:.2f}" if a.current_value is not None else "",
                "Receipt on File": "Yes" if a.has_receipt else "No",
                "Date Added": a.date_added[:10],
                "Notes": a.notes,
            })
