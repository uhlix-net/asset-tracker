"""Writes test fixtures in the formats older Asset Tracker versions produced.

    python make_fixtures.py <fixture-dir>

These formats are fixed: existing users have data written this way, so the C++
app must keep reading it. Nothing here should change to match new app behaviour.
"""
import base64
import json
import os
import pathlib
import re
import shutil
import sqlite3
import sys

import pyzipper
import win32crypt
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.hashes import SHA256
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from PIL import Image
from reportlab.lib.pagesizes import LETTER
from reportlab.pdfgen import canvas

out = pathlib.Path(sys.argv[1])
shutil.rmtree(out, ignore_errors=True)
data_dir = out / "legacy" / "AssetTracker"
assets_dir = data_dir / "assets"
(data_dir / "db").mkdir(parents=True)
src = out / "source"
src.mkdir()

BACKUP_PASSWORD = "correct horse"
SYNC_PASSWORD = "sync pässword"
DPAPI_HEADER = bytes.fromhex("01000000d08c9ddf0115d1118c7a00c04fc297eb")

# Folder names on disk, as older versions computed them. Frozen, not computed:
# the C++ slugify() must keep matching these.
SLUGS = {
    'Samsung 65" TV': "samsung_65_tv",
    "  Leading and trailing  ": "leading_and_trailing",
    "under_score-dash mix": "under_score_dash_mix",
    "A/B\\C:D*E?": "abcde",
    "Café Espresso Machine — Ünï": "café_espresso_machine_ünï",
    "日本語のテレビ": "日本語のテレビ",
    "x" * 60: "x" * 40,
    "Émile's 🎸 guitar": "émiles_guitar",
    "Dots...and   spaces": "dotsand_spaces",
    "---": "_",
    "": "",
}


def slugify(text):
    text = re.sub(r"[^\w\s-]", "", text.lower().strip())
    return re.sub(r"[\s_-]+", "_", text)[:40]


assert all(slugify(k) == v for k, v in SLUGS.items()), "slug reference drifted"


def protect(data):
    return win32crypt.CryptProtectData(data, None, None, None, None, 0)


def image(name, colour, size=(1600, 1200), exif_orientation=None):
    img = Image.new("RGB", size, colour)
    for x in range(size[0] // 2):
        for y in range(size[1] // 10):
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
    path = src / name
    c = canvas.Canvas(str(path), pagesize=LETTER)
    for page in range(pages):
        c.setFont("Helvetica", 24)
        c.drawString(72, 700, f"RECEIPT {name} PAGE {page + 1}")
        c.showPage()
    c.save()
    return path


db = sqlite3.connect(data_dir / "db" / "tracker.db")
db.executescript("""
    CREATE TABLE assets (
        id TEXT PRIMARY KEY, name TEXT NOT NULL, category TEXT NOT NULL DEFAULT '',
        date_purchase TEXT, value_estimate REAL, current_value REAL,
        serial_number TEXT NOT NULL DEFAULT '', model_number TEXT NOT NULL DEFAULT '',
        has_receipt INTEGER NOT NULL DEFAULT 0, date_added TEXT NOT NULL,
        notes TEXT NOT NULL DEFAULT '');
    CREATE TABLE asset_files (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        asset_id TEXT NOT NULL REFERENCES assets(id) ON DELETE CASCADE,
        file_name TEXT NOT NULL, file_type TEXT NOT NULL, stored_name TEXT NOT NULL,
        date_added TEXT NOT NULL, encrypted INTEGER NOT NULL DEFAULT 0);
    CREATE INDEX idx_af_asset_id ON asset_files(asset_id);
""")

ASSETS = [
    ("00001", 'Samsung 65" TV', "Living Room", "2024-03-15", 1234567.89, 999.5, "SN-1", "QN65",
     1, "2026-01-02T03:04:05Z", "Wall mounted.\nSecond line of notes."),
    ("00002", "Café Espresso Machine — Ünï", "Kitchen", None, None, None, "", "",
     1, "2026-01-03T03:04:05Z", ""),
    ("00003", "Uncategorized widget", "", "2020-01-01", 20.0, None, "", "",
     0, "2026-01-04T03:04:05Z", "x" * 400),
]
db.executemany("INSERT INTO assets VALUES (?,?,?,?,?,?,?,?,?,?,?)", ASSETS)


def add(asset, paths, kind, encrypted=True):
    folder = assets_dir / f"{asset[0]}_{slugify(asset[1])}"
    folder.mkdir(parents=True, exist_ok=True)
    for path in paths:
        raw = path.read_bytes()
        (folder / path.name).write_bytes(protect(raw) if encrypted else raw)
        db.execute("INSERT INTO asset_files (asset_id, file_name, file_type, stored_name, "
                   "date_added, encrypted) VALUES (?,?,?,?,?,?)",
                   (asset[0], path.name, kind, path.name, asset[9], int(encrypted)))


add(ASSETS[0], [image("front.jpg", (200, 30, 30)),
                image("rotated.jpg", (30, 200, 30), exif_orientation=6),
                image("back.png", (30, 30, 200)), image("side.webp", (200, 200, 30))], "image")
add(ASSETS[0], [receipt_pdf("tv_receipt.pdf", 2)], "receipt")
add(ASSETS[1], [image("espresso.jpg", (120, 80, 40), (800, 600))], "image")
add(ASSETS[1], [image("espresso_receipt.jpg", (240, 240, 240), (850, 1100))], "receipt")
add(ASSETS[2], [image("old.png", (90, 90, 90), (640, 480))], "image", encrypted=False)
db.commit()
db.close()


def write_backup(path, entries):
    with pyzipper.AESZipFile(path, "w", compression=pyzipper.ZIP_DEFLATED,
                             encryption=pyzipper.WZ_AES) as zf:
        zf.setpassword(BACKUP_PASSWORD.encode())
        for name, payload in entries:
            zf.writestr(name, payload)


# Format 1 backup: db/ and assets/ as stored, DPAPI files included, no format entry.
legacy = [(p.relative_to(data_dir).as_posix(), p.read_bytes())
          for p in sorted(data_dir.rglob("*")) if p.is_file()]
write_backup(out / "legacy_backup.zip", legacy)

# The same, with one file locked to a Windows account this one is not.
write_backup(out / "locked_backup.zip",
             [(n, DPAPI_HEADER + b"\0" * 200 if n.endswith("front.jpg") else b)
              for n, b in legacy])

# Hostile and useless archives.
db_bytes = (data_dir / "db" / "tracker.db").read_bytes()
write_backup(out / "evil_backup.zip", [("db/tracker.db", db_bytes),
                                       ("../escaped.txt", b"should never be written"),
                                       ("assets/../../escaped2.txt", b"should never be written")])
write_backup(out / "not_a_backup.zip", [("hello.txt", b"hi")])
(out / "garbage.zip").write_bytes(b"this is not a zip file")

# Sync encryption, as the Android app implements it.
key = PBKDF2HMAC(algorithm=SHA256(), length=32, salt=b"AssetTrackerSync_v1_uhlix",
                 iterations=600_000).derive(SYNC_PASSWORD.encode())


def encrypt(plain):
    nonce = os.urandom(12)
    return nonce + AESGCM(key).encrypt(nonce, plain, None)


meta = {"id": "00009", "name": "Reference blob ✓", "value_estimate": "1234.5",
        "current_value": "", "has_receipt": "1", "date_added": "2026-01-01T00:00:00Z"}
expected = {
    "backup_password": BACKUP_PASSWORD,
    "sync_password": SYNC_PASSWORD,
    "sync_key_hex": key.hex(),
    "sync_blob": base64.b64encode(encrypt(json.dumps(meta, ensure_ascii=False).encode())).decode(),
    "file_blob": base64.b64encode(encrypt(b"file bytes")).decode(),
    "slugs": SLUGS,
    "originals": {
        "front.jpg": (src / "front.jpg").read_bytes().hex(),
        "old.png": (src / "old.png").read_bytes().hex(),
    },
}
(out / "expected.json").write_text(json.dumps(expected, ensure_ascii=False), encoding="utf-8")
print("fixtures written to", out)
