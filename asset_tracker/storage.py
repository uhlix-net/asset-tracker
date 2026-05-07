from __future__ import annotations
import pathlib
import shutil
from datetime import datetime, timezone

from .config import ASSETS_DIR, IMAGE_EXTENSIONS, THUMB_SIZE
from .models import Asset, AssetFile


def _now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def asset_dir(asset: Asset) -> pathlib.Path:
    return ASSETS_DIR / asset.asset_dir_name


def ensure_asset_dir(asset: Asset) -> pathlib.Path:
    d = asset_dir(asset)
    d.mkdir(parents=True, exist_ok=True)
    return d


def import_files(
    asset: Asset,
    source_paths: list[pathlib.Path],
    file_type: str,
) -> list[AssetFile]:
    dest_dir = ensure_asset_dir(asset)
    files: list[AssetFile] = []
    for src in source_paths:
        dest = dest_dir / src.name
        # Avoid name collisions by appending a counter
        counter = 1
        while dest.exists():
            dest = dest_dir / f"{src.stem}_{counter}{src.suffix}"
            counter += 1
        shutil.copy2(src, dest)
        files.append(
            AssetFile(
                id=None,
                asset_id=asset.id,
                file_name=src.name,
                file_type=file_type,
                stored_name=dest.name,
                date_added=_now(),
            )
        )
    return files


def delete_asset_files(asset: Asset) -> None:
    d = asset_dir(asset)
    if d.exists():
        shutil.rmtree(d)


def get_stored_path(asset: Asset, af: AssetFile) -> pathlib.Path:
    return asset_dir(asset) / af.stored_name


def generate_thumbnail(file_path: pathlib.Path, size: tuple[int, int] = THUMB_SIZE):
    """Return a QPixmap thumbnail, or None if the file is not a supported image."""
    if file_path.suffix.lower() not in IMAGE_EXTENSIONS:
        return None
    try:
        from PIL import Image, ImageOps
        from PyQt6.QtGui import QPixmap, QImage
        import io

        with Image.open(file_path) as img:
            img = ImageOps.exif_transpose(img)
            img.thumbnail(size, Image.LANCZOS)
            img = img.convert("RGBA")
            buf = io.BytesIO()
            img.save(buf, format="PNG")
            buf.seek(0)
            data = buf.read()

        qimg = QImage.fromData(data)
        return QPixmap.fromImage(qimg)
    except Exception:
        return None
