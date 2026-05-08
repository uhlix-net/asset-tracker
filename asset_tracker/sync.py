"""
Firebase cloud sync for Asset Tracker.

Encryption model:
  - PBKDF2-HMAC-SHA256 (600 000 iterations) derives a 256-bit key
    from the user-chosen sync password.
  - Every asset metadata record and every file is AES-256-GCM
    encrypted BEFORE being sent to Firebase.  Google only ever
    stores ciphertext; the sync password never leaves the device.

Firebase layout (under users/{uid}/):
  Firestore documents:
    assets/{asset_id}        — {"data": <base64 AES-GCM JSON>, "updated": <iso>}
    metadata/file_manifest   — {"data": <base64 AES-GCM JSON>, "updated": <iso>}

  Storage objects:
    files/{asset_id}/{stored_name}   — AES-256-GCM encrypted file bytes
"""
from __future__ import annotations

import base64
import json
import os
import time
from datetime import datetime, timezone
from typing import Callable

import requests
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.hashes import SHA256
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC

from . import storage as file_storage

# ── Key derivation ────────────────────────────────────────────────────────────

_SALT       = b"AssetTrackerSync_v1_uhlix"
_ITERATIONS = 600_000


def derive_key(password: str) -> bytes:
    """Derive a 256-bit AES key from the sync password."""
    kdf = PBKDF2HMAC(algorithm=SHA256(), length=32,
                     salt=_SALT, iterations=_ITERATIONS)
    return kdf.derive(password.encode("utf-8"))


# ── AES-256-GCM helpers ───────────────────────────────────────────────────────

def encrypt(plaintext: bytes, key: bytes) -> bytes:
    nonce = os.urandom(12)
    ct = AESGCM(key).encrypt(nonce, plaintext, None)
    return nonce + ct           # 12-byte nonce prepended


def decrypt(ciphertext: bytes, key: bytes) -> bytes:
    return AESGCM(key).decrypt(ciphertext[:12], ciphertext[12:], None)


def encrypt_json(obj: dict, key: bytes) -> str:
    raw = json.dumps(obj, ensure_ascii=False).encode("utf-8")
    return base64.b64encode(encrypt(raw, key)).decode("ascii")


def decrypt_json(b64: str, key: bytes) -> dict:
    raw = decrypt(base64.b64decode(b64), key)
    return json.loads(raw.decode("utf-8"))


# ── Firebase REST client ──────────────────────────────────────────────────────

class FirebaseSync:
    _AUTH      = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword"
    _STORAGE   = "https://firebasestorage.googleapis.com/v0/b/{bucket}/o"
    _FIRESTORE = ("https://firestore.googleapis.com/v1/projects/{project}"
                  "/databases/(default)/documents")

    def __init__(self, api_key: str, project_id: str, bucket: str) -> None:
        self.api_key    = api_key
        self.project_id = project_id
        self.bucket     = bucket
        self._token:    str | None = None
        self._uid:      str | None = None
        self._session   = requests.Session()
        self._session.headers["Content-Type"] = "application/json"

    # ── Auth ──────────────────────────────────────────────────────────────────

    def authenticate(self, email: str, password: str) -> None:
        resp = self._session.post(
            f"{self._AUTH}?key={self.api_key}",
            json={"email": email, "password": password,
                  "returnSecureToken": True},
            timeout=15,
        )
        if resp.status_code != 200:
            msg = resp.json().get("error", {}).get("message", resp.text[:120])
            raise RuntimeError(f"Firebase authentication failed: {msg}")
        data          = resp.json()
        self._token   = data["idToken"]
        self._uid     = data["localId"]

    def _hdr(self) -> dict:
        return {"Authorization": f"Bearer {self._token}"}

    # ── Firestore helpers ─────────────────────────────────────────────────────

    def _fs_doc_url(self, *parts: str) -> str:
        base = self._FIRESTORE.format(project=self.project_id)
        path = "/".join(["users", self._uid, *parts])
        return f"{base}/{path}"

    def _fs_set(self, *parts: str, fields: dict) -> None:
        url  = self._fs_doc_url(*parts)
        body = {"fields": {k: {"stringValue": v} for k, v in fields.items()}}
        r    = self._session.patch(url, headers=self._hdr(), json=body, timeout=15)
        if r.status_code not in (200, 201):
            raise RuntimeError(f"Firestore write failed ({r.status_code}): {r.text[:120]}")

    def _fs_list(self, collection: str) -> list[dict]:
        url = self._fs_doc_url(collection)
        r   = self._session.get(url, headers=self._hdr(), timeout=15)
        if r.status_code == 404:
            return []
        if r.status_code != 200:
            raise RuntimeError(f"Firestore read failed ({r.status_code}): {r.text[:120]}")
        return r.json().get("documents", [])

    def _fs_delete(self, *parts: str) -> None:
        self._session.delete(self._fs_doc_url(*parts),
                             headers=self._hdr(), timeout=15)

    # ── Storage helpers ───────────────────────────────────────────────────────

    def _st_path(self, rel: str) -> str:
        return f"users/{self._uid}/{rel}"

    def _st_upload(self, rel: str, data: bytes) -> None:
        name    = self._st_path(rel)
        encoded = requests.utils.quote(name, safe="")
        url     = (f"{self._STORAGE.format(bucket=self.bucket)}"
                   f"?uploadType=media&name={encoded}")
        r = self._session.post(
            url,
            headers={**self._hdr(), "Content-Type": "application/octet-stream"},
            data=data,
            timeout=120,
        )
        if r.status_code not in (200, 201):
            raise RuntimeError(f"Storage upload failed ({r.status_code}): {r.text[:120]}")

    def _st_download(self, rel: str) -> bytes:
        name    = self._st_path(rel)
        encoded = requests.utils.quote(name, safe="")
        url     = f"{self._STORAGE.format(bucket=self.bucket)}/{encoded}?alt=media"
        r       = self._session.get(url, headers=self._hdr(), timeout=120)
        if r.status_code != 200:
            raise RuntimeError(f"Storage download failed ({r.status_code})")
        return r.content

    # ── Push all assets ───────────────────────────────────────────────────────

    def push_all(
        self,
        db,
        sync_key: bytes,
        progress: Callable[[str], None] | None = None,
    ) -> dict:
        """Encrypt and push all assets + files to Firebase."""
        assets         = db.get_all_assets()
        total          = len(assets)
        pushed_assets  = 0
        pushed_files   = 0
        errors: list[str] = []
        file_manifest: dict = {}

        for i, asset in enumerate(assets):
            if progress:
                progress(f"Uploading {i + 1}/{total}: {asset.name}")
            try:
                meta = {
                    "id":            asset.id,
                    "name":          asset.name,
                    "category":      asset.category or "",
                    "date_purchase": asset.date_purchase or "",
                    "value_estimate": str(asset.value_estimate) if asset.value_estimate is not None else "",
                    "current_value": str(asset.current_value) if asset.current_value is not None else "",
                    "serial_number": asset.serial_number or "",
                    "model_number":  asset.model_number or "",
                    "has_receipt":   str(int(asset.has_receipt)),
                    "date_added":    asset.date_added,
                    "notes":         asset.notes or "",
                }
                self._fs_set("assets", asset.id,
                             fields={"data":    encrypt_json(meta, sync_key),
                                     "updated": datetime.now(timezone.utc).isoformat()})
                pushed_assets += 1

                af_list = db.get_asset_files(asset.id)
                file_manifest[asset.id] = [
                    {"file_name":   af.file_name,
                     "stored_name": af.stored_name,
                     "file_type":   af.file_type}
                    for af in af_list
                ]
                for af in af_list:
                    src = file_storage.get_stored_path(asset, af)
                    if src.exists():
                        raw = file_storage.read_file_bytes(src, encrypted=af.encrypted)
                        self._st_upload(f"files/{asset.id}/{af.stored_name}",
                                        encrypt(raw, sync_key))
                        pushed_files += 1
            except Exception as exc:
                errors.append(f"{asset.id}: {exc}")

        if progress:
            progress("Writing manifest…")
        self._fs_set("metadata", "manifest",
                     fields={"data":    encrypt_json(file_manifest, sync_key),
                              "updated": datetime.now(timezone.utc).isoformat()})

        return {"assets": pushed_assets, "files": pushed_files, "errors": errors}

    # ── Download helpers for Android (used by tests / future restore) ─────────

    def fetch_asset_list(self, sync_key: bytes) -> list[dict]:
        docs   = self._fs_list("assets")
        result = []
        for doc in docs:
            enc = doc.get("fields", {}).get("data", {}).get("stringValue", "")
            if enc:
                try:
                    result.append(decrypt_json(enc, sync_key))
                except Exception:
                    pass
        return result

    def fetch_file(self, asset_id: str, stored_name: str, sync_key: bytes) -> bytes:
        enc = self._st_download(f"files/{asset_id}/{stored_name}")
        return decrypt(enc, sync_key)
