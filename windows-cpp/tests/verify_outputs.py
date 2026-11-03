"""Checks what the C++ tests wrote, independently of the C++ code. The decryption
here mirrors android_app/lib/services/crypto_service.dart, so anything it reads,
the Android app can too.

    python verify_outputs.py <fixture-dir> <output-dir> <mock-port>
"""
import base64
import json
import pathlib
import sys
import urllib.request

import pyzipper
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

fixtures, out = map(pathlib.Path, sys.argv[1:3])
port = sys.argv[3]
expected = json.loads((fixtures / "expected.json").read_text(encoding="utf-8"))
key = bytes.fromhex(expected["sync_key_hex"])


def decrypt(blob):
    return AESGCM(key).decrypt(blob[:12], blob[12:], None)


def decrypt_json(b64):
    return json.loads(decrypt(base64.b64decode(b64)).decode("utf-8"))


with pyzipper.AESZipFile(out / "cpp_backup.zip") as z:
    z.setpassword(expected["backup_password"].encode())
    names = z.namelist()
    assert "db/tracker.db" in names and "backup_format.json" in names, names
    assert json.loads(z.read("backup_format.json"))["format"] == 2
    front = next(n for n in names if n.endswith("/front.jpg"))
    assert z.read(front).hex() == expected["originals"]["front.jpg"], "backup file is not plain"
print("ok  backup opens as a standard AES zip and holds decrypted files")

blob = json.loads((out / "cpp_sync.json").read_text(encoding="utf-8"))
assert decrypt_json(blob["json"])["name"] == "C++ blob ✓"
assert decrypt(base64.b64decode(blob["file"])) == b"file bytes"
print("ok  AES-GCM blobs decrypt with the Android scheme")

dump = json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/__dump"))
# Documents are keyed "<uid>/<collection>/<id>"; the reference account is uid1.
assets = {k.split("/", 1)[1]: decrypt_json(v["data"]["stringValue"])
          for k, v in dump["docs"].items() if k.startswith("uid1/assets/")}
tv = assets["assets/00001"]
assert tv["value_estimate"] == "1234567.89", tv["value_estimate"]
assert float(tv["current_value"]) == 999.5 and tv["has_receipt"] == "1"
assert assets["assets/00002"]["value_estimate"] == ""

manifest = decrypt_json(dump["docs"]["uid1/metadata/manifest"]["data"]["stringValue"])
stored = manifest["00001"][0]["stored_name"]
raw = base64.b64decode(dump["objects"][f"users/uid1/files/00001/{stored}"])
assert len(decrypt(raw)) > 0
print("ok  cloud records and files read back with the Android scheme")
