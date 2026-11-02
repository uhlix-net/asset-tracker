"""Checks what the C++ tests wrote, using the Python edition's own code. Anything it
can read, the Android app can too: crypto_service.dart mirrors sync.py.

    python verify_outputs.py <python-repo-root> <fixture-dir> <output-dir> <mock-port>
"""
import base64
import json
import pathlib
import sys
import urllib.request

repo, fixtures, out = map(pathlib.Path, sys.argv[1:4])
sys.path.insert(0, str(repo))
from asset_tracker import sync
import pyzipper

expected = json.loads((fixtures / "expected.json").read_text(encoding="utf-8"))

with pyzipper.AESZipFile(out / "cpp_backup.zip") as z:
    z.setpassword(expected["backup_password"].encode())
    names = z.namelist()
    assert "db/tracker.db" in names and "backup_format.json" in names, names
    assert json.loads(z.read("backup_format.json"))["format"] == 2
    front = next(n for n in names if n.endswith("/front.jpg"))
    assert z.read(front).hex() == expected["originals"]["front.jpg"], "backup file is not plain"
print("ok  C++ backup opens with pyzipper and holds decrypted files")

key = bytes.fromhex(expected["sync_key_hex"])
blob = json.loads((out / "cpp_sync.json").read_text(encoding="utf-8"))
assert sync.decrypt_json(blob["json"], key)["name"] == "C++ blob ✓"
assert sync.decrypt(base64.b64decode(blob["file"]), key) == b"file bytes"
print("ok  C++ AES-GCM blobs decrypt in Python")

port = sys.argv[4]
dump = json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/__dump"))
assets = {k: sync.decrypt_json(v["data"]["stringValue"], key) for k, v in dump["docs"].items()
          if k.startswith("assets/")}
tv = assets["assets/00001"]
assert tv["value_estimate"] == "1234567.89", tv["value_estimate"]
assert float(tv["current_value"]) == 999.5 and tv["has_receipt"] == "1"
assert assets["assets/00002"]["value_estimate"] == ""

manifest = sync.decrypt_json(dump["docs"]["metadata/manifest"]["data"]["stringValue"], key)
stored = manifest["00001"][0]["stored_name"]
raw = base64.b64decode(dump["objects"][f"users/uid1/files/00001/{stored}"])
assert len(sync.decrypt(raw, key)) > 0
print("ok  cloud records and files read back with the Android-compatible scheme")
