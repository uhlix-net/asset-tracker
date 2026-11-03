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
    int assets = 0;                 // Uploaded this time.
    int files = 0;
    int unchangedAssets = 0;        // Already in the cloud and unchanged.
    int unchangedFiles = 0;
    QStringList errors;
    QVector<CloudAsset> cloudOnly;  // In the cloud, not on this PC. Left in place.
    bool keyMismatch = false;       // Nothing was sent: see allowKeyChange.
    SyncState state;                // Store this; pass it to the next push.
};

// Uploads what changed since the last push, judged by state and by what the cloud
// actually holds, so anything missing there is sent again. Removes cloud files this
// PC no longer has for its own assets. Assets missing locally are never deleted
// here, only reported, so an empty or partial install cannot wipe the only
// remaining copy. Unless allowKeyChange is set, stops before writing when no cloud
// record decrypts, which means the sync password differs from the one in use.
PushResult push(const FirebaseConfig &config, const QString &syncPassword,
                const QVector<AssetRecord> &records, const SyncState &state,
                bool allowKeyChange, const Progress &progress);

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
// Windows account, with sync state recorded so the next push sends nothing.
RestoreResult downloadToStaging(const FirebaseConfig &config, const QString &syncPassword,
                                const Progress &progress);

// ── Accounts ─────────────────────────────────────────────────────────────────

void signIn(const FirebaseConfig &config);           // Throws with a user-facing reason.
void createAccount(const FirebaseConfig &config);
void sendPasswordReset(const FirebaseConfig &config, const QString &email);
void changeAccountPassword(const FirebaseConfig &config, const QString &newPassword);

enum class KeyCheck { NoCloudData, Matches, DoesNotMatch };
KeyCheck checkSyncPassword(const FirebaseConfig &config, const QString &syncPassword);

// Everything under users/{uid}/: records, manifest and files. The account stays.
void deleteAllCloudData(const FirebaseConfig &config, const Progress &progress);

// deleteAllCloudData(), then the login itself.
void deleteAccount(const FirebaseConfig &config, const Progress &progress);

// The string-typed record the Android app parses. Exposed for tests.
QJsonObject assetToJson(const Asset &asset);
Asset assetFromJson(const QJsonObject &json);

} // namespace Sync
