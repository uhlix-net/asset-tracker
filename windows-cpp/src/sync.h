#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

#include "firebaseclient.h"
#include "models.h"

// End-to-end encrypted cloud copy. Every record and file is AES-256-GCM encrypted
// before it leaves the machine; Google only ever stores ciphertext.
//
// Firebase layout, shared with the Android app (under users/{uid}/):
//   assets/{asset_id}              {"data": <base64 JSON>, "updated": <iso>}
//   metadata/manifest              {"data": <base64 JSON>, "updated": <iso>}
//   files/{asset_id}/{stored_name} (Storage) encrypted file bytes
//
// All functions block, take no Database, and belong on a worker thread.
namespace Sync {

using Progress = std::function<void(const QString &)>;

struct CloudAsset {
    QString id;
    QString name;   // Empty when the record cannot be decrypted.
};

struct PushResult {
    int assets = 0;
    int files = 0;
    QStringList errors;
    QVector<CloudAsset> cloudOnly;  // In the cloud, not on this PC. Left in place.
    bool keyMismatch = false;       // Nothing was sent: see allowKeyChange.
};

// Uploads every local asset and removes cloud files this PC no longer has for
// those assets. Assets missing locally are never deleted here, only reported, so
// an empty or partial install cannot wipe the only remaining copy. Unless
// allowKeyChange is set, stops before writing when no cloud record decrypts,
// which means the sync password differs from the one already in use.
PushResult push(const FirebaseConfig &config, const QString &syncPassword,
                const QVector<AssetRecord> &records, bool allowKeyChange,
                const Progress &progress);

// Removes these assets, their files and their manifest entries from the cloud.
void deleteFromCloud(const FirebaseConfig &config, const QString &syncPassword,
                     const QStringList &assetIds, const Progress &progress);

struct RestoreResult {
    QString stagingDir;     // Complete data tree, ready for Backup::commitRestore().
    int assets = 0;
    int files = 0;
    QStringList errors;     // Items that could not be fetched. Staging is still usable.
};

// Downloads the whole cloud copy into a staging tree, re-encrypted for this
// Windows account. Nothing live is touched.
RestoreResult downloadToStaging(const FirebaseConfig &config, const QString &syncPassword,
                                const Progress &progress);

// The string-typed record the Android app parses. Exposed for tests.
QJsonObject assetToJson(const Asset &asset);
Asset assetFromJson(const QJsonObject &json);

} // namespace Sync
