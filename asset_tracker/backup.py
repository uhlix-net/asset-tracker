from __future__ import annotations
import pathlib
import shutil
from datetime import date

from .config import DB_DIR, ASSETS_DIR, DATA_DIR


def default_backup_name() -> str:
    return f"assets_backup_{date.today().strftime('%Y-%m-%d')}.zip"


def create_backup(password: str, dest: str | pathlib.Path) -> None:
    """Create an AES-256 encrypted ZIP backup of db/ and assets/."""
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

        if DB_DIR.exists():
            for f in DB_DIR.rglob("*"):
                if f.is_file():
                    zf.write(f, f.relative_to(DB_DIR.parent))

        if ASSETS_DIR.exists():
            for f in ASSETS_DIR.rglob("*"):
                if f.is_file():
                    zf.write(f, f.relative_to(ASSETS_DIR.parent))


def restore_backup(password: str, src: str | pathlib.Path) -> None:
    """
    Restore from an AES-256 encrypted ZIP backup.
    Existing data is moved to a timestamped backup folder first,
    then the archive contents are extracted into DATA_DIR.
    """
    import pyzipper
    from datetime import datetime

    src = pathlib.Path(src)

    # Verify the password works before touching existing data
    with pyzipper.AESZipFile(src, mode="r") as zf:
        zf.setpassword(password.encode("utf-8"))
        # Reading the name list forces decryption header check
        names = zf.namelist()

    # Move current data aside
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    if DATA_DIR.exists():
        backup_aside = DATA_DIR.parent / f"{DATA_DIR.name}_pre_restore_{stamp}"
        shutil.move(str(DATA_DIR), str(backup_aside))

    DATA_DIR.mkdir(parents=True, exist_ok=True)

    with pyzipper.AESZipFile(src, mode="r") as zf:
        zf.setpassword(password.encode("utf-8"))
        zf.extractall(path=DATA_DIR.parent)
