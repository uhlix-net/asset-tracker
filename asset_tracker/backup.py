from __future__ import annotations
import pathlib
from datetime import date

from .config import DB_DIR, ASSETS_DIR


def default_backup_name() -> str:
    return f"assets_backup_{date.today().strftime('%Y-%m-%d')}.zip"


def create_backup(password: str, dest: str | pathlib.Path) -> None:
    """Create an AES-256 encrypted ZIP backup of the db/ and assets/ directories."""
    import pyzipper

    dest = pathlib.Path(dest)
    dest.parent.mkdir(parents=True, exist_ok=True)

    with pyzipper.AESZipFile(
        dest,
        mode="w",
        compression=pyzipper.ZIP_DEFLATED,
        encryption=pyzipper.WZ_AES,
    ) as zf:
        zf.setpassword(password.encode("utf-8"))

        # Back up db/
        if DB_DIR.exists():
            for f in DB_DIR.rglob("*"):
                if f.is_file():
                    arcname = f.relative_to(DB_DIR.parent)
                    zf.write(f, arcname)

        # Back up assets/
        if ASSETS_DIR.exists():
            for f in ASSETS_DIR.rglob("*"):
                if f.is_file():
                    arcname = f.relative_to(ASSETS_DIR.parent)
                    zf.write(f, arcname)
