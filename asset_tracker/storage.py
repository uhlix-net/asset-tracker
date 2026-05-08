from __future__ import annotations
import io
import pathlib
import shutil
from datetime import datetime, timezone

from .config import ASSETS_DIR, IMAGE_EXTENSIONS, THUMB_SIZE
from .models import Asset, AssetFile

# ── DPAPI availability ────────────────────────────────────────────────────────
try:
    import win32crypt as _win32crypt
    _DPAPI = True
except ImportError:
    _DPAPI = False   # Linux / development environment — files stored unencrypted


def _now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# ── DPAPI helpers ─────────────────────────────────────────────────────────────

def dpapi_available() -> bool:
    return _DPAPI


def encrypt_bytes(data: bytes) -> bytes:
    """Encrypt bytes with Windows DPAPI (current user scope).
    Returns data unchanged when DPAPI is unavailable."""
    if not _DPAPI:
        return data
    return _win32crypt.CryptProtectData(data, None, None, None, None, 0)


def decrypt_bytes(data: bytes) -> bytes:
    """Decrypt DPAPI-encrypted bytes.
    Falls back to returning data unchanged if decryption fails (e.g. legacy
    unencrypted file) or DPAPI is unavailable."""
    if not _DPAPI:
        return data
    try:
        _desc, plain = _win32crypt.CryptUnprotectData(data, None, None, None, 0)
        return plain
    except Exception:
        return data   # Not encrypted — return as-is (migration fallback)


def read_file_bytes(path: pathlib.Path, encrypted: bool = False) -> bytes:
    """Read a file, decrypting it if the encrypted flag is set."""
    raw = path.read_bytes()
    return decrypt_bytes(raw) if encrypted else raw


# ── Directory helpers ─────────────────────────────────────────────────────────

def asset_dir(asset: Asset) -> pathlib.Path:
    return ASSETS_DIR / asset.asset_dir_name


def ensure_asset_dir(asset: Asset) -> pathlib.Path:
    d = asset_dir(asset)
    d.mkdir(parents=True, exist_ok=True)
    return d


# ── File import ───────────────────────────────────────────────────────────────

def import_files(
    asset: Asset,
    source_paths: list[pathlib.Path],
    file_type: str,
) -> list[AssetFile]:
    dest_dir = ensure_asset_dir(asset)
    files: list[AssetFile] = []
    for src in source_paths:
        dest = dest_dir / src.name
        counter = 1
        while dest.exists():
            dest = dest_dir / f"{src.stem}_{counter}{src.suffix}"
            counter += 1

        raw = src.read_bytes()
        if _DPAPI:
            dest.write_bytes(encrypt_bytes(raw))
        else:
            shutil.copy2(src, dest)

        files.append(
            AssetFile(
                id=None,
                asset_id=asset.id,
                file_name=src.name,
                file_type=file_type,
                stored_name=dest.name,
                date_added=_now(),
                encrypted=_DPAPI,
            )
        )
    return files


# ── File deletion ─────────────────────────────────────────────────────────────

def delete_asset_files(asset: Asset) -> None:
    d = asset_dir(asset)
    if d.exists():
        shutil.rmtree(d)


def get_stored_path(asset: Asset, af: AssetFile) -> pathlib.Path:
    return asset_dir(asset) / af.stored_name


# ── Export (unencrypted) ──────────────────────────────────────────────────────

def export_asset_files(assets: list[Asset], db, dest_dir: pathlib.Path) -> int:
    """Decrypt and copy all files for each asset into dest_dir.
    Returns the number of files exported."""
    count = 0
    for asset in assets:
        files = db.get_asset_files(asset.id)
        if not files:
            continue
        out = dest_dir / asset.asset_dir_name
        out.mkdir(parents=True, exist_ok=True)
        for af in files:
            src = get_stored_path(asset, af)
            if src.exists():
                (out / af.stored_name).write_bytes(
                    read_file_bytes(src, encrypted=af.encrypted)
                )
                count += 1
    return count


# ── Thumbnail generation ──────────────────────────────────────────────────────

def generate_thumbnail(
    file_path: pathlib.Path,
    size: tuple[int, int] = THUMB_SIZE,
    encrypted: bool = False,
):
    """Return a QPixmap thumbnail or None if the file is not a supported image."""
    if file_path.suffix.lower() not in IMAGE_EXTENSIONS:
        return None
    try:
        from PIL import Image, ImageOps
        from PyQt6.QtGui import QPixmap, QImage

        raw = read_file_bytes(file_path, encrypted=encrypted)
        with Image.open(io.BytesIO(raw)) as img:
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
