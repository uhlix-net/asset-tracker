#include "crypto.h"

#include <QJsonDocument>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <memory>
#include <stdexcept>

namespace {

const char kSalt[] = "AssetTrackerSync_v1_uhlix";
constexpr int kIterations = 600000;
constexpr int kKeySize = 32;
constexpr int kNonceSize = 12;
constexpr int kTagSize = 16;

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

const unsigned char *bytes(const QByteArray &data) {
    return reinterpret_cast<const unsigned char *>(data.constData());
}

unsigned char *bytes(QByteArray &data) {
    return reinterpret_cast<unsigned char *>(data.data());
}

} // namespace

QByteArray Crypto::deriveKey(const QString &password) {
    const QByteArray utf8 = password.toUtf8();
    QByteArray key(kKeySize, Qt::Uninitialized);
    if (PKCS5_PBKDF2_HMAC(utf8.constData(), utf8.size(),
                          reinterpret_cast<const unsigned char *>(kSalt), sizeof(kSalt) - 1,
                          kIterations, EVP_sha256(), kKeySize, bytes(key)) != 1)
        throw std::runtime_error("Could not derive the sync key.");
    return key;
}

QByteArray Crypto::encrypt(const QByteArray &plain, const QByteArray &key) {
    QByteArray out(kNonceSize + plain.size() + kTagSize, Qt::Uninitialized);
    unsigned char *nonce = bytes(out);
    unsigned char *cipher = nonce + kNonceSize;
    if (RAND_bytes(nonce, kNonceSize) != 1)
        throw std::runtime_error("Could not generate a nonce.");

    CipherContext ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int len = 0, finalLen = 0;
    if (!ctx
        || EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, bytes(key), nonce) != 1
        || EVP_EncryptUpdate(ctx.get(), cipher, &len, bytes(plain), plain.size()) != 1
        || EVP_EncryptFinal_ex(ctx.get(), cipher + len, &finalLen) != 1
        || EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, kTagSize,
                               cipher + len + finalLen) != 1)
        throw std::runtime_error("Encryption failed.");
    return out;
}

QByteArray Crypto::decrypt(const QByteArray &data, const QByteArray &key) {
    if (data.size() < kNonceSize + kTagSize)
        throw std::runtime_error("Encrypted data is truncated.");

    const int cipherSize = data.size() - kNonceSize - kTagSize;
    QByteArray tag = data.right(kTagSize);
    QByteArray out(cipherSize, Qt::Uninitialized);

    CipherContext ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int len = 0, finalLen = 0;
    if (!ctx
        || EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, bytes(key), bytes(data)) != 1
        || EVP_DecryptUpdate(ctx.get(), bytes(out), &len, bytes(data) + kNonceSize, cipherSize) != 1
        || EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, kTagSize, tag.data()) != 1
        || EVP_DecryptFinal_ex(ctx.get(), bytes(out) + len, &finalLen) != 1)
        throw std::runtime_error("Could not decrypt: wrong sync password or damaged data.");
    return out;
}

QString Crypto::encryptJson(const QJsonObject &object, const QByteArray &key) {
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(encrypt(json, key).toBase64());
}

QJsonObject Crypto::decryptJson(const QString &base64, const QByteArray &key) {
    const QByteArray plain = decrypt(QByteArray::fromBase64(base64.toLatin1()), key);
    const QJsonDocument doc = QJsonDocument::fromJson(plain);
    if (!doc.isObject()) throw std::runtime_error("Decrypted data is not valid JSON.");
    return doc.object();
}
