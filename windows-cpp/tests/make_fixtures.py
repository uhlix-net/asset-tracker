"""Generates test fixtures by running the real Python edition's code.

Run with a Windows Python that has requirements.txt installed:
    python make_fixtures.py <python-repo-root> <fixture-dir>

Everything the C++ edition must stay compatible with comes out of the code that
wrote it: the database, DPAPI files, backups, sync blobs and slugs.
"""
import io
import json
import os
import pathlib
import shutil
import sys
import zipfile

repo, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
shutil.rmtree(out, ignore_errors=True)
data_root = out / "legacy"
data_root.mkdir(parents=True)
os.environ["APPDATA"] = str(data_root)   # config.py reads this at import time
sys.path.insert(0, str(repo))

from PIL import Image, PngImagePlugin

from asset_tracker import backup, report, storage, sync
from asset_tracker.config import DATA_DIR
from asset_tracker.database import Database
from asset_tracker.models import Asset, AssetFile, slugify

src = out / "source"
src.mkdir()

def image(name, colour, size=(1600, 1200), exif_orientation=None):
    img = Image.new("RGB", size, colour)
    for x in range(0, size[0] // 2):
        for y in range(0, size[1] // 10):
            img.putpixel((x, y), (255, 255, 255))
    path = src / name
    if exif_orientation:
        exif = Image.Exif()
        exif[0x0112] = exif_orientation
        img.save(path, exif=exif)
    else:
        img.save(path)
    return path

def receipt_pdf(name, pages):
    from reportlab.lib.pagesizes import LETTER
    from reportlab.pdfgen import canvas
    path = src / name
    c = canvas.Canvas(str(path), pagesize=LETTER)
    for page in range(pages):
        c.setFont("Helvetica", 24)
        c.drawString(72, 700, f"RECEIPT {name} PAGE {page + 1}")
        c.showPage()
    c.save()
    return path

db = Database()
assets = [
    Asset(id="00001", name='Samsung 65" TV', category="Living Room", date_purchase="2024-03-15",
          value_estimate=1234567.89, current_value=999.5, serial_number="SN-1",
          model_number="QN65", has_receipt=True, date_added="2026-01-02T03:04:05Z",
          notes="Wall mounted.\nSecond line of notes."),
    Asset(id="00002", name="Café Espresso Machine — Ünï", category="Kitchen", date_purchase=None,
          value_estimate=None, current_value=None, serial_number="", model_number="",
          has_receipt=True, date_added="2026-01-03T03:04:05Z", notes=""),
    Asset(id="00003", name="Uncategorized widget", category="", date_purchase="2020-01-01",
          value_estimate=20.0, current_value=None, serial_number="", model_number="",
          has_receipt=False, date_added="2026-01-04T03:04:05Z", notes="x" * 400),
]
for asset in assets:
    db.insert_asset(asset)

def add(asset, paths, kind):
    for f in storage.import_files(asset, paths, kind):
        db.insert_asset_file(f)

add(assets[0], [image("front.jpg", (200, 30, 30)), image("rotated.jpg", (30, 200, 30), exif_orientation=6),
                image("back.png", (30, 30, 200)), image("side.webp", (200, 200, 30))], "image")
add(assets[0], [receipt_pdf("tv_receipt.pdf", 2)], "receipt")
add(assets[1], [image("espresso.jpg", (120, 80, 40), (800, 600))], "image")
add(assets[1], [image("espresso_receipt.jpg", (240, 240, 240), (850, 1100))], "receipt")

# A file from before DPAPI encryption was added.
plain_dir = storage.asset_dir(assets[2])
plain_dir.mkdir(parents=True)
shutil.copy(image("old.png", (90, 90, 90), (640, 480)), plain_dir / "old.png")
db.insert_asset_file(AssetFile(id=None, asset_id="00003", file_name="old.png", file_type="image",
                               stored_name="old.png", date_added="2026-01-04T03:04:05Z",
                               encrypted=False))

records = db.get_all_assets_with_files()
report.generate_report(records, out / "python_report.pdf",
                       {"company": "Acme Mutual", "policy_number": "PO-1", "claim_number": "",
                        "date_of_loss": "May 1, 2026"})
db.close()

PASSWORD = "correct horse"
backup.create_backup(PASSWORD, out / "legacy_backup.zip")

# The same backup with one file replaced by a DPAPI blob this account cannot open,
# which is what restoring on another Windows account looks like.
import pyzipper
with pyzipper.AESZipFile(out / "legacy_backup.zip") as zin, \
     pyzipper.AESZipFile(out / "locked_backup.zip", "w", compression=pyzipper.ZIP_DEFLATED,
                         encryption=pyzipper.WZ_AES) as zo:
    zin.setpassword(PASSWORD.encode())
    zo.setpassword(PASSWORD.encode())
    for name in zin.namelist():
        payload = zin.read(name)
        if name.endswith("front.jpg"):
            payload = bytes.fromhex("01000000d08c9ddf0115d1118c7a00c04fc297eb") + b"\0" * 200
        zo.writestr(name, payload)

# Hostile and useless archives.
with pyzipper.AESZipFile(out / "evil_backup.zip", "w", compression=pyzipper.ZIP_DEFLATED,
                         encryption=pyzipper.WZ_AES) as zo:
    zo.setpassword(PASSWORD.encode())
    with pyzipper.AESZipFile(out / "legacy_backup.zip") as zin:
        zin.setpassword(PASSWORD.encode())
        zo.writestr("db/tracker.db", zin.read("db/tracker.db"))
    zo.writestr("../escaped.txt", b"should never be written")
    zo.writestr("assets/../../escaped2.txt", b"should never be written")
with pyzipper.AESZipFile(out / "not_a_backup.zip", "w", encryption=pyzipper.WZ_AES) as zo:
    zo.setpassword(PASSWORD.encode())
    zo.writestr("hello.txt", b"hi")
(out / "garbage.zip").write_bytes(b"this is not a zip file")

SYNC_PASSWORD = "sync pässword"
key = sync.derive_key(SYNC_PASSWORD)
meta = {"id": "00009", "name": "Python blob ✓", "value_estimate": "1234.5", "current_value": "",
        "has_receipt": "1", "date_added": "2026-01-01T00:00:00Z"}

expected = {
    "backup_password": PASSWORD,
    "sync_password": SYNC_PASSWORD,
    "sync_key_hex": key.hex(),
    "sync_blob": sync.encrypt_json(meta, key),
    "file_blob": __import__("base64").b64encode(sync.encrypt(b"file bytes", key)).decode(),
    "slugs": {name: slugify(name) for name in [
        'Samsung 65" TV', "  Leading and trailing  ", "under_score-dash mix", "A/B\\C:D*E?",
        "Café Espresso Machine — Ünï", "日本語のテレビ", "x" * 60, "Émile's 🎸 guitar",
        "Dots...and   spaces", "---", "",
    ]},
    "originals": {
        "front.jpg": (src / "front.jpg").read_bytes().hex(),
        "old.png": (src / "old.png").read_bytes().hex(),
    },
}
(out / "expected.json").write_text(json.dumps(expected, ensure_ascii=False), encoding="utf-8")
print("fixtures written to", out)
