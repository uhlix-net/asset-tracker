#include "sync.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <stdexcept>

#include "backup.h"
#include "crypto.h"
#include "database.h"
#include "storage.h"

namespace {

QString decimal(std::optional<double> value) {
    // Never QString::number(value): its default is 6 significant digits, which
    // would turn 1234567.89 into 1.23457e+06 in the cloud.
    return value ? QString::fromLatin1(QByteArray::number(*value, 'f',
                                                          QLocale::FloatingPointShortest))
                 : QString();
}

std::optional<double> parseDecimal(const QString &text) {
    bool ok = false;
    const double value = text.toDouble(&ok);
    return ok ? std::optional<double>(value) : std::nullopt;
}

QJsonArray manifestEntry(const QVector<AssetFile> &files) {
    QJsonArray entry;
    for (const AssetFile &file : files) {
        entry.append(QJsonObject{
            {"file_name", file.fileName},
            {"stored_name", file.storedName},
            {"file_type", file.fileType},
        });
    }
    return entry;
}

QJsonObject readManifest(FirebaseClient &client, const QByteArray &key) {
    const QString data = client.getDocument("metadata", "manifest");
    if (data.isEmpty()) return QJsonObject();
    try {
        return Crypto::decryptJson(data, key);
    } catch (const std::exception &) {
        return QJsonObject();
    }
}

QString fingerprint(const QByteArray &data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString jsonFingerprint(const QJsonObject &object) {
    return fingerprint(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

// Ties remembered fingerprints to one project, account and sync key.
QString stateTarget(const FirebaseConfig &config, const QString &uid, const QByteArray &key) {
    return fingerprint(config.projectId.toUtf8() + '\n' + uid.toUtf8() + '\n'
                       + QCryptographicHash::hash(key, QCryptographicHash::Sha256));
}

QString assetKey(const QString &id) { return "asset/" + id; }

QString fileKey(const QString &id, const QString &stored) {
    return QString("file/%1/%2").arg(id, stored);
}

void removeCloudObjects(FirebaseClient &client, const QString &assetId) {
    const QStringList objects = client.listObjects(QString("files/%1/").arg(assetId));
    for (const QString &object : objects)
        client.deleteObject(object);
}

// Names in the cloud are authenticated by the sync key, but they still become
// paths on this PC, so they get the same treatment as archive entries.
bool isSafeName(const QString &name) {
    static const QRegularExpression unsafe(R"([/\\:]|^\.\.?$|^$)");
    return !name.contains(unsafe);
}

} // namespace

QJsonObject Sync::assetToJson(const Asset &asset) {
    return QJsonObject{
        {"id", asset.id},
        {"name", asset.name},
        {"category", asset.category},
        {"date_purchase", asset.datePurchase},
        {"value_estimate", decimal(asset.valueEstimate)},
        {"current_value", decimal(asset.currentValue)},
        {"serial_number", asset.serialNumber},
        {"model_number", asset.modelNumber},
        {"has_receipt", asset.hasReceipt ? "1" : "0"},
        {"date_added", asset.dateAdded},
        {"notes", asset.notes},
    };
}

Asset Sync::assetFromJson(const QJsonObject &json) {
    Asset asset;
    asset.id            = json.value("id").toString();
    asset.name          = json.value("name").toString();
    asset.category      = json.value("category").toString();
    asset.datePurchase  = json.value("date_purchase").toString();
    asset.valueEstimate = parseDecimal(json.value("value_estimate").toString());
    asset.currentValue  = parseDecimal(json.value("current_value").toString());
    asset.serialNumber  = json.value("serial_number").toString();
    asset.modelNumber   = json.value("model_number").toString();
    asset.hasReceipt    = json.value("has_receipt").toString() == "1";
    asset.dateAdded     = json.value("date_added").toString();
    asset.notes         = json.value("notes").toString();
    return asset;
}

Sync::PushResult Sync::push(const FirebaseConfig &config, const QString &syncPassword,
                            const QVector<AssetRecord> &records, const SyncState &state,
                            bool allowKeyChange, const Progress &progress) {
    PushResult result;

    progress("Deriving encryption key…");
    const QByteArray key = Crypto::deriveKey(syncPassword);

    FirebaseClient client(config);
    progress("Signing in…");
    client.authenticate();

    progress("Checking the cloud copy…");
    const QVector<FirebaseClient::Document> remote = client.listDocuments("assets");

    QHash<QString, QString> remoteNames;
    int readable = 0;
    for (const FirebaseClient::Document &document : remote) {
        QString name;
        try {
            name = Crypto::decryptJson(document.data, key).value("name").toString();
            ++readable;
        } catch (const std::exception &) {}
        remoteNames.insert(document.id, name);
    }

    if (!remote.isEmpty() && readable == 0 && !allowKeyChange) {
        result.keyMismatch = true;
        result.state = state;
        return result;
    }
    if (records.isEmpty() && !remote.isEmpty()) {
        throw std::runtime_error(
            "This PC has no assets, but the cloud does. Nothing was changed. "
            "Use Restore from Cloud to bring them back.");
    }

    // Fingerprints only count for the same project, account and key, and only
    // while the cloud still holds what they describe.
    const QString target = stateTarget(config, client.uid(), key);
    const QHash<QString, QString> known = (state.target == target && !allowKeyChange)
        ? state.fingerprints : QHash<QString, QString>();
    result.state.target = target;
    QHash<QString, QString> &next = result.state.fingerprints;

    const QStringList listed = client.listObjects("files/");
    const QSet<QString> remoteObjects(listed.cbegin(), listed.cend());

    QSet<QString> localIds;
    for (const AssetRecord &record : records)
        localIds.insert(record.asset.id);

    QJsonObject manifest;
    const QJsonObject oldManifest = readManifest(client, key);
    for (auto it = remoteNames.cbegin(); it != remoteNames.cend(); ++it) {
        if (localIds.contains(it.key())) continue;
        result.cloudOnly.append({it.key(), it.value()});
        // Without its entry, a cloud-only asset would lose its files in a later restore.
        if (oldManifest.contains(it.key()))
            manifest.insert(it.key(), oldManifest.value(it.key()));
    }

    QHash<QString, QSet<QString>> expected;
    const int total = records.size();
    for (int i = 0; i < total; ++i) {
        const Asset &asset = records[i].asset;
        progress(QString("Checking %1/%2: %3").arg(i + 1).arg(total).arg(asset.name));

        QSet<QString> &names = expected[asset.id];
        for (const AssetFile &file : records[i].files)
            names.insert(file.storedName);
        manifest.insert(asset.id, manifestEntry(records[i].files));

        const QJsonObject json = assetToJson(asset);
        const QString assetFp = jsonFingerprint(json);
        if (remoteNames.contains(asset.id) && known.value(assetKey(asset.id)) == assetFp) {
            next.insert(assetKey(asset.id), assetFp);
            ++result.unchangedAssets;
        } else {
            try {
                client.setDocument("assets", asset.id, Crypto::encryptJson(json, key));
                next.insert(assetKey(asset.id), assetFp);
                ++result.assets;
            } catch (const std::exception &e) {
                result.errors << QString("%1: %2").arg(asset.id, e.what());
                continue;
            }
        }

        for (const AssetFile &file : records[i].files) {
            const QString path = Storage::storedPath(asset, file);
            if (!QFileInfo::exists(path)) continue;
            const QString object = QString("files/%1/%2").arg(asset.id, file.storedName);
            const QString stateKey = fileKey(asset.id, file.storedName);
            try {
                const QByteArray stored = Storage::readFile(path, false);
                const QString fileFp = fingerprint(stored);
                if (remoteObjects.contains(object) && known.value(stateKey) == fileFp) {
                    next.insert(stateKey, fileFp);
                    ++result.unchangedFiles;
                    continue;
                }
                progress(QString("Uploading %1/%2: %3").arg(i + 1).arg(total).arg(file.fileName));
                const QByteArray plain = file.encrypted ? Storage::unprotect(stored) : stored;
                client.upload(object, Crypto::encrypt(plain, key));
                next.insert(stateKey, fileFp);
                ++result.files;
            } catch (const std::exception &e) {
                result.errors << QString("%1/%2: %3").arg(asset.id, file.fileName, e.what());
            }
        }
    }

    const QString manifestFp = jsonFingerprint(manifest);
    if (oldManifest.isEmpty() || known.value("manifest") != manifestFp) {
        progress("Writing manifest…");
        client.setDocument("metadata", "manifest", Crypto::encryptJson(manifest, key));
    }
    next.insert("manifest", manifestFp);

    progress("Removing deleted files from the cloud…");
    for (const QString &object : remoteObjects) {
        const QString assetId = object.section('/', 1, 1);
        const QString name = object.section('/', 2);
        if (!localIds.contains(assetId) || expected[assetId].contains(name)) continue;
        try {
            client.deleteObject(object);
        } catch (const std::exception &e) {
            result.errors << e.what();
        }
    }
    return result;
}

void Sync::deleteFromCloud(const FirebaseConfig &config, const QString &syncPassword,
                           const QStringList &assetIds, const Progress &progress) {
    const QByteArray key = Crypto::deriveKey(syncPassword);
    FirebaseClient client(config);
    client.authenticate();

    QJsonObject manifest = readManifest(client, key);
    for (const QString &id : assetIds) {
        progress(QString("Deleting %1 from the cloud…").arg(id));
        removeCloudObjects(client, id);
        client.deleteDocument("assets", id);
        manifest.remove(id);
    }
    client.setDocument("metadata", "manifest", Crypto::encryptJson(manifest, key));
}

Sync::RestoreResult Sync::downloadToStaging(const FirebaseConfig &config,
                                            const QString &syncPassword,
                                            const Progress &progress) {
    progress("Deriving encryption key…");
    const QByteArray key = Crypto::deriveKey(syncPassword);

    FirebaseClient client(config);
    progress("Authenticating with Firebase…");
    client.authenticate();

    progress("Reading the cloud copy…");
    const QVector<FirebaseClient::Document> remote = client.listDocuments("assets");
    if (remote.isEmpty())
        throw std::runtime_error("There are no assets in the cloud to restore.");

    RestoreResult result;
    QVector<Asset> assets;
    for (const FirebaseClient::Document &document : remote) {
        try {
            assets.append(assetFromJson(Crypto::decryptJson(document.data, key)));
        } catch (const std::exception &) {
            result.errors << QString("Asset %1 could not be decrypted.").arg(document.id);
        }
    }
    if (assets.isEmpty())
        throw std::runtime_error("Could not decrypt the cloud copy. Check the sync password.");

    const QJsonObject manifest = readManifest(client, key);
    result.stagingDir = Backup::newStagingDir();

    // What was downloaded is by definition what the cloud holds, so the next push
    // can skip it all.
    SyncState state;
    state.target = stateTarget(config, client.uid(), key);
    state.fingerprints.insert("manifest", jsonFingerprint(manifest));

    try {
        Database db(result.stagingDir + "/db/tracker.db");
        const QString assetsRoot = result.stagingDir + "/assets";

        const int total = assets.size();
        for (int i = 0; i < total; ++i) {
            const Asset &asset = assets[i];
            progress(QString("Downloading %1/%2: %3").arg(i + 1).arg(total).arg(asset.name));
            if (!isSafeName(asset.id)) {
                result.errors << QString("Skipped an asset with an invalid ID.");
                continue;
            }
            db.insertAsset(asset);
            state.fingerprints.insert(assetKey(asset.id), jsonFingerprint(assetToJson(asset)));
            ++result.assets;

            const QJsonArray entries = manifest.value(asset.id).toArray();
            for (const QJsonValue &entry : entries) {
                const QJsonObject object = entry.toObject();
                AssetFile file;
                file.assetId    = asset.id;
                file.fileName   = object.value("file_name").toString();
                file.storedName = object.value("stored_name").toString();
                file.fileType   = object.value("file_type").toString();
                file.dateAdded  = asset.dateAdded;
                file.encrypted  = true;

                if (!isSafeName(file.storedName)) {
                    result.errors << QString("%1: skipped an unsafe file name.").arg(asset.id);
                    continue;
                }
                try {
                    const QByteArray plain = Crypto::decrypt(
                        client.download(QString("files/%1/%2").arg(asset.id, file.storedName)),
                        key);
                    const QByteArray sealed = Storage::protect(plain);
                    Storage::writeFile(assetsRoot + "/" + asset.dirName() + "/" + file.storedName,
                                       sealed);
                    db.insertAssetFile(file);
                    state.fingerprints.insert(fileKey(asset.id, file.storedName),
                                              fingerprint(sealed));
                    ++result.files;
                } catch (const std::exception &e) {
                    result.errors << QString("%1/%2: %3").arg(asset.id, file.fileName, e.what());
                }
            }
        }
        db.saveSyncState(state);
    } catch (...) {
        QDir(result.stagingDir).removeRecursively();
        throw;
    }
    return result;
}

void Sync::signIn(const FirebaseConfig &config) {
    FirebaseClient(config).authenticate();
}

void Sync::createAccount(const FirebaseConfig &config) {
    FirebaseClient(config).createAccount();
}

void Sync::sendPasswordReset(const FirebaseConfig &config, const QString &email) {
    FirebaseClient(config).sendPasswordReset(email);
}

void Sync::changeAccountPassword(const FirebaseConfig &config, const QString &newPassword) {
    FirebaseClient client(config);
    client.authenticate();
    client.changePassword(newPassword);
}

Sync::KeyCheck Sync::checkSyncPassword(const FirebaseConfig &config, const QString &syncPassword) {
    FirebaseClient client(config);
    client.authenticate();
    const QVector<FirebaseClient::Document> remote = client.listDocuments("assets");
    if (remote.isEmpty()) return KeyCheck::NoCloudData;

    const QByteArray key = Crypto::deriveKey(syncPassword);
    for (const FirebaseClient::Document &document : remote) {
        try {
            Crypto::decryptJson(document.data, key);
            return KeyCheck::Matches;
        } catch (const std::exception &) {}
    }
    return KeyCheck::DoesNotMatch;
}

namespace {

void deleteEverything(FirebaseClient &client, const Sync::Progress &progress) {
    progress("Deleting files…");
    const QStringList objects = client.listObjects(QString());
    for (const QString &object : objects)
        client.deleteObject(object);

    progress("Deleting records…");
    const QVector<FirebaseClient::Document> documents = client.listDocuments("assets");
    for (const FirebaseClient::Document &document : documents)
        client.deleteDocument("assets", document.id);
    client.deleteDocument("metadata", "manifest");
}

} // namespace

void Sync::deleteAllCloudData(const FirebaseConfig &config, const Progress &progress) {
    FirebaseClient client(config);
    progress("Signing in…");
    client.authenticate();
    deleteEverything(client, progress);
}

void Sync::deleteAccount(const FirebaseConfig &config, const Progress &progress) {
    FirebaseClient client(config);
    progress("Signing in…");
    client.authenticate();
    deleteEverything(client, progress);
    progress("Deleting the account…");
    client.deleteAccount();
}
