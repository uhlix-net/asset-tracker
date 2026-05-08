from __future__ import annotations
from dataclasses import dataclass, field
import re


def slugify(text: str) -> str:
    text = text.lower().strip()
    text = re.sub(r"[^\w\s-]", "", text)
    text = re.sub(r"[\s_-]+", "_", text)
    return text[:40]


@dataclass
class Asset:
    id: str                      # Zero-padded 5-digit string: "00001"
    name: str
    category: str                # e.g. "Living Room", "Kitchen"
    date_purchase: str | None    # "YYYY-MM-DD" or None
    value_estimate: float | None # Purchase / estimated value
    current_value: float | None  # Current / appraised value
    serial_number: str           # Optional serial number
    model_number: str            # Optional model number
    has_receipt: bool
    date_added: str              # ISO-8601 UTC
    notes: str = ""

    @property
    def asset_dir_name(self) -> str:
        return f"{self.id}_{slugify(self.name)}"

    @property
    def value_display(self) -> str:
        if self.value_estimate is None:
            return "—"
        return f"${self.value_estimate:,.2f}"

    @property
    def current_value_display(self) -> str:
        if self.current_value is None:
            return "—"
        return f"${self.current_value:,.2f}"


@dataclass
class AssetFile:
    id: int | None               # None before DB insert
    asset_id: str
    file_name: str               # Original filename
    file_type: str               # "image" | "receipt"
    stored_name: str             # Filename inside asset's directory
    date_added: str              # ISO-8601 UTC
    encrypted: bool = False      # True when stored with Windows DPAPI
