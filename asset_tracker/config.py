import os
import pathlib

APP_NAME = "AssetTracker"
APP_VERSION = "1.0.3"

if os.name == "nt":
    _base = pathlib.Path(os.environ["APPDATA"])
else:
    _base = pathlib.Path.home() / ".local" / "share"

DATA_DIR = _base / APP_NAME
DB_DIR = DATA_DIR / "db"
DB_PATH = DB_DIR / "tracker.db"
ASSETS_DIR = DATA_DIR / "assets"

THUMB_SIZE = (160, 160)

IMAGE_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp", ".tiff", ".tif", ".ico",
}

RECEIPT_EXTENSIONS = {
    ".pdf", ".png", ".jpg", ".jpeg", ".tiff", ".tif",
}

ASSET_CATEGORIES = [
    "Living Room",
    "Bedroom",
    "Kitchen",
    "Dining Room",
    "Bathroom",
    "Office / Study",
    "Garage",
    "Basement / Attic",
    "Outdoor / Patio",
    "Other",
]
