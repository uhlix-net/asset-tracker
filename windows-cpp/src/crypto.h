#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>

// The end-to-end sync scheme. The Android app implements the same thing in
// crypto_service.dart, so the salt, iteration count and blob layout here must
// never change on one side only.
namespace Crypto {

// PBKDF2-HMAC-SHA256, 600 000 iterations, 256-bit key. Slow: not on the UI thread.
QByteArray deriveKey(const QString &password);

// AES-256-GCM. Layout: [12-byte nonce][ciphertext][16-byte tag].
QByteArray encrypt(const QByteArray &plain, const QByteArray &key);
QByteArray decrypt(const QByteArray &data, const QByteArray &key);  // Throws on a bad key or tag.

QString encryptJson(const QJsonObject &object, const QByteArray &key);
QJsonObject decryptJson(const QString &base64, const QByteArray &key);

} // namespace Crypto
