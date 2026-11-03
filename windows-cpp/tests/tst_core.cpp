// Tests for the data layer and for compatibility with what older versions wrote. Fixtures come from make_fixtures.py; run everything with run_tests.py.
//
//   ASSET_TRACKER_FIXTURES  fixture directory
//   ASSET_TRACKER_TEST_OUT  where this writes files for verify_outputs.py
//   ASSET_TRACKER_MOCK      base URL of mock_firebase.py
//   ASSET_TRACKER_REAL_DATA optional: an AssetTracker folder to read-test. Only a
//                           temporary copy of it is opened.
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleHints>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPdfDocument>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

#include "aboutdialog.h"
#include "assetform.h"
#include "assetlist.h"
#include "backup.h"
#include "config.h"
#include "crypto.h"
#include "csvexport.h"
#include "database.h"
#include "firebaseclient.h"
#include "helpdialog.h"
#include "insurerinfodialog.h"
#include "mainwindow.h"
#include "previewpanel.h"
#include "report.h"
#include "secretstore.h"
#include "storage.h"
#include "sync.h"
#include "cloudconfig.h"
#include "cloudsetupdialog.h"
#include "preferencesdialog.h"
#include "syncdialog.h"
#include "theme.h"
#include "updatehistorydialog.h"

namespace {

QJsonObject readJson(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void copyTree(const QString &from, const QString &to) {
    QDir().mkpath(to);
    const QFileInfoList entries = QDir(from).entryInfoList(QDir::Files | QDir::Dirs
                                                           | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        const QString target = to + "/" + entry.fileName();
        if (entry.isDir()) copyTree(entry.filePath(), target);
        else QFile::copy(entry.filePath(), target);
    }
}

int countFiles(const QString &root) {
    int count = 0;
    const QFileInfoList entries = QDir(root).entryInfoList(QDir::Files | QDir::Dirs
                                                           | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries)
        count += entry.isDir() ? countFiles(entry.filePath()) : 1;
    return count;
}

QByteArray fromHex(const QString &hex) { return QByteArray::fromHex(hex.toLatin1()); }

} // namespace

class TestCore : public QObject {
    Q_OBJECT

    QString m_fixtures, m_out;
    QJsonObject m_expected;
    std::unique_ptr<QTemporaryDir> m_root;

    QString fixture(const QString &name) const { return m_fixtures + "/" + name; }

    // A private data tree: empty, or a copy of data in the older formats.
    void useRoot(bool withLegacyData) {
        m_root = std::make_unique<QTemporaryDir>();
        Config::setTestRoot(m_root->path());
        if (withLegacyData) copyTree(fixture("legacy/AssetTracker"), Config::dataDir());
    }

    QStringList strayFolders() const {
        return QDir(m_root->path()).entryList({"AssetTracker_*"}, QDir::Dirs);
    }

    FirebaseConfig mockConfig() const {
        FirebaseConfig config;
        config.apiKey = "test-api-key";
        config.projectId = "project";
        config.bucket = "bucket";
        config.email = "a@b.c";
        config.password = "fb-password";
        config.endpointOverride = qEnvironmentVariable("ASSET_TRACKER_MOCK");
        return config;
    }

    QByteArray mockCall(const QByteArray &verb, const QString &path) {
        QNetworkAccessManager network;
        QNetworkRequest request{QUrl(qEnvironmentVariable("ASSET_TRACKER_MOCK") + path)};
        std::unique_ptr<QNetworkReply> reply(network.sendCustomRequest(request, verb));
        QEventLoop loop;
        connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        return reply->readAll();
    }

    static void noProgress(const QString &) {}

private slots:
    void initTestCase() {
        m_fixtures = qEnvironmentVariable("ASSET_TRACKER_FIXTURES");
        m_out = qEnvironmentVariable("ASSET_TRACKER_TEST_OUT");
        QVERIFY2(!m_fixtures.isEmpty() && !m_out.isEmpty(), "fixture variables not set");
        QDir().mkpath(m_out);
        m_expected = readJson(fixture("expected.json"));
        QVERIFY(!m_expected.isEmpty());
    }

    void slugsMatchExistingFolders() {
        const QJsonObject slugs = m_expected.value("slugs").toObject();
        for (auto it = slugs.begin(); it != slugs.end(); ++it)
            QCOMPARE(slugify(it.key()), it.value().toString());
    }

    void money() {
        QCOMPARE(moneyDisplay(1234567.891), QStringLiteral("$1,234,567.89"));
        QCOMPARE(moneyDisplay(0.0), QStringLiteral("$0.00"));
        QCOMPARE(moneyDisplay(std::nullopt), QStringLiteral("—"));
    }

    void readsExistingDatabaseAndFiles() {
        useRoot(true);
        Database db(Config::dbPath());

        const QVector<Asset> assets = db.getAllAssets();
        QCOMPARE(assets.size(), 3);
        const auto tv = db.getAssetById("00001");
        QVERIFY(tv);
        QCOMPARE(tv->name, QStringLiteral("Samsung 65\" TV"));
        QCOMPARE(*tv->valueEstimate, 1234567.89);
        QCOMPARE(*tv->currentValue, 999.5);
        QVERIFY(tv->hasReceipt);
        QVERIFY(!db.getAssetById("00002")->valueEstimate);
        QCOMPARE(db.nextAssetId(), QStringLiteral("00004"));

        const QVector<AssetFile> files = db.getAssetFiles("00001");
        QCOMPARE(files.size(), 5);
        for (const AssetFile &file : files) {
            QVERIFY(file.encrypted);
            QVERIFY(QFile::exists(Storage::storedPath(*tv, file)));
        }
        const AssetFile *front = nullptr;
        for (const AssetFile &file : files) {
            if (file.fileName == "front.jpg") front = &file;
        }
        QVERIFY(front);
        QCOMPARE(Storage::readFile(Storage::storedPath(*tv, *front), true),
                 fromHex(m_expected.value("originals").toObject().value("front.jpg").toString()));

        // A file from before encryption, flagged plain.
        const auto widget = db.getAssetById("00003");
        const AssetFile old = db.getAssetFiles("00003").first();
        QVERIFY(!old.encrypted);
        QCOMPARE(Storage::readFile(Storage::storedPath(*widget, old), false),
                 fromHex(m_expected.value("originals").toObject().value("old.png").toString()));
    }

    void folderFoundWhenSlugDiffers() {
        useRoot(true);
        Asset asset;
        asset.id = "00002";
        asset.name = "A completely different name";
        QVERIFY(QFileInfo(Storage::assetDir(asset)).fileName().startsWith("00002_caf"));
    }

    void imagesLoadAndRotate() {
        useRoot(true);
        Database db(Config::dbPath());
        const auto tv = db.getAssetById("00001");
        const QVector<AssetFile> files = db.getAssetFiles("00001");
        for (const AssetFile &file : files) {
            if (file.fileType != "image") continue;
            const QImage image = Storage::loadImage(Storage::storedPath(*tv, file), true);
            QVERIFY2(!image.isNull(), qPrintable(file.fileName));
            // EXIF orientation 6 turns the 1600x1200 source on its side.
            if (file.fileName == "rotated.jpg") QVERIFY(image.height() > image.width());
            else QVERIFY(image.width() > image.height());
        }
    }

    void foreignBlobFailsInsteadOfPassingThrough() {
        const QByteArray blob = fromHex("01000000d08c9ddf0115d1118c7a00c04fc297eb")
                                + QByteArray(200, '\0');
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, Storage::unprotect(blob));
        QCOMPARE(Storage::unprotect("plain data"), QByteArray("plain data"));
        QCOMPARE(Storage::unprotect(Storage::protect("secret")), QByteArray("secret"));
    }

    void addRenameDeleteRoundTrip() {
        useRoot(false);
        Database db(Config::dbPath());
        Asset asset;
        asset.id = db.nextAssetId();
        QCOMPARE(asset.id, QStringLiteral("00001"));
        asset.name = "New Thing";
        asset.dateAdded = utcNow();
        db.insertAsset(asset);

        const QString source = m_root->filePath("photo.jpg");
        QVERIFY(QFile::copy(fixture("source/front.jpg"), source));
        const QVector<AssetFile> first = Storage::importFiles(asset, {source, source}, "image");
        QCOMPARE(first[0].storedName, QStringLiteral("photo.jpg"));
        QCOMPARE(first[1].storedName, QStringLiteral("photo_1.jpg"));
        for (const AssetFile &file : first) db.insertAssetFile(file);

        QFile stored(Storage::storedPath(asset, first[0]));
        QVERIFY(stored.open(QIODevice::ReadOnly));
        QVERIFY(Storage::isProtected(stored.readAll()));
        stored.close();

        Asset renamed = asset;
        renamed.name = "Renamed Thing";
        Storage::renameAssetDir(asset, renamed);
        db.updateAsset(renamed);
        QVERIFY(Storage::assetDir(renamed).endsWith("00001_renamed_thing"));
        QVERIFY(!Storage::readFile(Storage::storedPath(renamed, first[0]), true).isEmpty());

        Storage::deleteAssetFiles(renamed);
        db.deleteAsset(renamed.id);
        QVERIFY(db.getAssetFiles(renamed.id).isEmpty());   // cascades
        QCOMPARE(db.getAllAssets().size(), 0);
    }

    void restoresFormat1Backup() {
        useRoot(false);
        const QString staging = Backup::prepareRestore(
            m_expected.value("backup_password").toString(), fixture("legacy_backup.zip"));
        Database db(Config::dbPath());
        Backup::commitRestore(staging, db);

        QCOMPARE(db.getAllAssets().size(), 3);
        const auto tv = db.getAssetById("00001");
        for (const AssetFile &file : db.getAssetFiles("00001"))
            QVERIFY(!Storage::readFile(Storage::storedPath(*tv, file), file.encrypted).isEmpty());
    }

    void wrongPasswordLeavesDataAlone() {
        useRoot(true);
        const int before = countFiles(Config::dataDir());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
            Backup::prepareRestore("wrong password", fixture("legacy_backup.zip")));
        QCOMPARE(countFiles(Config::dataDir()), before);
        QVERIFY(strayFolders().isEmpty());
    }

    void rejectsBadArchives() {
        useRoot(true);
        const QString password = m_expected.value("backup_password").toString();
        const int before = countFiles(Config::dataDir());
        for (const char *name : {"locked_backup.zip", "not_a_backup.zip", "garbage.zip"}) {
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                Backup::prepareRestore(password, fixture(name)));
        }
        QCOMPARE(countFiles(Config::dataDir()), before);
        QVERIFY(strayFolders().isEmpty());
    }

    void archiveCannotEscapeStaging() {
        useRoot(false);
        const QString staging = Backup::prepareRestore(
            m_expected.value("backup_password").toString(), fixture("evil_backup.zip"));
        QVERIFY(QFile::exists(staging + "/db/tracker.db"));
        QVERIFY(!QFile::exists(m_root->filePath("escaped.txt")));
        QVERIFY(!QFile::exists(m_root->filePath("escaped2.txt")));
        QVERIFY(!QFile::exists(QFileInfo(m_root->path()).path() + "/escaped.txt"));
        QDir(staging).removeRecursively();
    }

    void backupRoundTrip() {
        useRoot(true);
        const QString password = m_expected.value("backup_password").toString();
        const QString dest = m_out + "/cpp_backup.zip";
        QFile::remove(dest);
        {
            Database db(Config::dbPath());
            Backup::create(password, dest, db.getAllAssetsWithFiles());
        }
        QVERIFY(QFile::exists(dest));
        QVERIFY(!QFile::exists(dest + ".part"));

        // Restores over live data; the old tree is kept aside.
        Database db(Config::dbPath());
        const QString staging = Backup::prepareRestore(password, dest);
        const QString aside = Backup::commitRestore(staging, db);
        QVERIFY(QFile::exists(aside + "/db/tracker.db"));
        QCOMPARE(db.getAllAssets().size(), 3);

        // Files were re-sealed with DPAPI, not left as the archive's plain copies.
        const auto tv = db.getAssetById("00001");
        const AssetFile file = db.getAssetFiles("00001").first();
        QFile stored(Storage::storedPath(*tv, file));
        QVERIFY(stored.open(QIODevice::ReadOnly));
        QVERIFY(Storage::isProtected(stored.readAll()));
    }

    void backupFailsRatherThanSkipping() {
        useRoot(true);
        const QString dest = m_out + "/should_not_exist.zip";
        Database db(Config::dbPath());
        const QVector<AssetRecord> records = db.getAllAssetsWithFiles();
        const auto tv = db.getAssetById("00001");
        const AssetFile file = db.getAssetFiles("00001").first();
        Storage::writeFile(Storage::storedPath(*tv, file),
                           fromHex("01000000d08c9ddf0115d1118c7a00c04fc297eb")
                               + QByteArray(200, '\0'));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, Backup::create("pw", dest, records));
        QVERIFY(!QFile::exists(dest));
        QVERIFY(!QFile::exists(dest + ".part"));
    }

    void cryptoMatchesAndroidScheme() {
        const QString password = m_expected.value("sync_password").toString();
        const QByteArray key = fromHex(m_expected.value("sync_key_hex").toString());
        QCOMPARE(Crypto::deriveKey(password), key);

        const QJsonObject blob = Crypto::decryptJson(m_expected.value("sync_blob").toString(), key);
        QCOMPARE(blob.value("name").toString(), QStringLiteral("Reference blob ✓"));
        QCOMPARE(Crypto::decrypt(QByteArray::fromBase64(
                     m_expected.value("file_blob").toString().toLatin1()), key),
                 QByteArray("file bytes"));

        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
            Crypto::decrypt(Crypto::encrypt("x", key), QByteArray(32, 'k')));

        const QJsonObject out{
            {"json", Crypto::encryptJson(QJsonObject{{"name", QStringLiteral("C++ blob ✓")}}, key)},
            {"file", QString::fromLatin1(Crypto::encrypt("file bytes", key).toBase64())},
        };
        Storage::writeFile(m_out + "/cpp_sync.json", QJsonDocument(out).toJson());
    }

    void syncRecordKeepsFullPrecision() {
        Asset asset;
        asset.valueEstimate = 1234567.89;
        asset.currentValue = 1000000.0;
        const QJsonObject json = Sync::assetToJson(asset);
        QCOMPARE(json.value("value_estimate").toString(), QStringLiteral("1234567.89"));
        QCOMPARE(json.value("current_value").toString(), QStringLiteral("1000000"));
        const Asset back = Sync::assetFromJson(json);
        QCOMPARE(*back.valueEstimate, 1234567.89);
        QVERIFY(!Sync::assetFromJson(Sync::assetToJson(Asset())).valueEstimate);
    }

    void passwordsLeavePlainTextOnStartup() {
        useRoot(false);
        {
            const auto settings = Config::openSettings();
            settings->setValue("firebase/password", "hunter2");
            settings->setValue("firebase/sync_password", "s3cret");
            settings->setValue("firebase/api_key", "stays");
        }
        CloudConfig::migrateStoredPasswords();
        const auto settings = Config::openSettings();
        QVERIFY(!settings->contains("firebase/password"));
        QVERIFY(!settings->contains("firebase/sync_password"));
        QCOMPARE(settings->value("firebase/api_key").toString(), QStringLiteral("stays"));
        QCOMPARE(SecretStore::unprotect(settings->value("firebase/password_enc").toString()),
                 QStringLiteral("hunter2"));
        QCOMPARE(SecretStore::unprotect(settings->value("firebase/sync_password_enc").toString()),
                 QStringLiteral("s3cret"));

        settings->setValue("firebase/password", "");     // Empty values just go.
        CloudConfig::migrateStoredPasswords();
        QVERIFY(!Config::openSettings()->contains("firebase/password"));
    }

    void cloudSettingsRoundTrip() {
        useRoot(false);
        QVERIFY(!CloudConfig::isLocked());
        QVERIFY(!CloudConfig::hasAccount());
        CloudConfig::saveProject({"key", "project", "bucket"});
        CloudConfig::saveAccount("me@example.com", "account pass");
        CloudConfig::saveSyncPassword("sync secret", true);
        QCOMPARE(CloudConfig::project().projectId, QStringLiteral("project"));
        QVERIFY(CloudConfig::hasAccount());
        QCOMPARE(CloudConfig::password(), QStringLiteral("account pass"));
        QCOMPARE(CloudConfig::syncPassword(), QStringLiteral("sync secret"));

        // Nothing secret is stored in the clear.
        QFile ini(m_root->filePath("settings.ini"));
        QVERIFY(ini.open(QIODevice::ReadOnly));
        const QByteArray raw = ini.readAll();
        QVERIFY(!raw.contains("account pass"));
        QVERIFY(!raw.contains("sync secret"));
        ini.close();

        CloudConfig::saveSyncPassword("sync secret", false);
        QVERIFY(CloudConfig::syncPassword().isEmpty());
        CloudConfig::signOut();
        QVERIFY(!CloudConfig::hasAccount());
        QCOMPARE(CloudConfig::project().projectId, QStringLiteral("project"));
    }

    void syncStateRoundTrip() {
        useRoot(false);
        Database db(Config::dbPath());
        QVERIFY(db.loadSyncState().fingerprints.isEmpty());
        SyncState state;
        state.target = "t";
        state.fingerprints.insert("asset/00001", "abc");
        state.fingerprints.insert("manifest", "def");
        db.saveSyncState(state);
        const SyncState loaded = db.loadSyncState();
        QCOMPARE(loaded.target, QStringLiteral("t"));
        QCOMPARE(loaded.fingerprints, state.fingerprints);
        db.saveSyncState(SyncState());
        QVERIFY(db.loadSyncState().target.isEmpty());
    }

    void syncSendsOnlyChanges() {
        if (qEnvironmentVariableIsEmpty("ASSET_TRACKER_MOCK")) QSKIP("mock not running");
        mockCall("POST", "/__reset");
        useRoot(true);
        const FirebaseConfig config = mockConfig();
        const QString password = m_expected.value("sync_password").toString();
        const auto progress = &TestCore::noProgress;

        Database db(Config::dbPath());
        QVector<AssetRecord> records = db.getAllAssetsWithFiles();

        Sync::PushResult first = Sync::push(config, password, records, SyncState(), false, progress);
        QCOMPARE(first.assets, 3);
        QCOMPARE(first.files, 8);

        // Nothing changed: nothing is sent.
        Sync::PushResult second = Sync::push(config, password, records, first.state, false, progress);
        QCOMPARE(second.assets, 0);
        QCOMPARE(second.files, 0);
        QCOMPARE(second.unchangedAssets, 3);
        QCOMPARE(second.unchangedFiles, 8);

        // An edited item sends that record only.
        records[1].asset.notes = "changed";
        Sync::PushResult third = Sync::push(config, password, records, second.state, false, progress);
        QCOMPARE(third.assets, 1);
        QCOMPARE(third.files, 0);

        // A new photo sends that file only.
        const Asset &tv = records[0].asset;
        const QString source = m_root->filePath("new.jpg");
        QVERIFY(QFile::copy(fixture("source/front.jpg"), source));
        for (const AssetFile &file : Storage::importFiles(tv, {source}, "image")) {
            db.insertAssetFile(file);
            records[0].files.append(file);
        }
        Sync::PushResult fourth = Sync::push(config, password, records, third.state, false, progress);
        QCOMPARE(fourth.assets, 0);
        QCOMPARE(fourth.files, 1);

        // Something missing from the cloud is sent again even though state says it was sent.
        FirebaseClient client(config);
        client.authenticate();
        client.deleteObject(QString("files/%1/%2").arg(tv.id, records[0].files.first().storedName));
        Sync::PushResult fifth = Sync::push(config, password, records, fourth.state, false, progress);
        QCOMPARE(fifth.files, 1);
        QCOMPARE(fifth.assets, 0);

        // State for another account or key is ignored: everything goes again.
        SyncState foreign = fifth.state;
        foreign.target = "someone else";
        Sync::PushResult sixth = Sync::push(config, password, records, foreign, false, progress);
        QCOMPARE(sixth.assets, 3);
        QCOMPARE(sixth.files, 9);

        // A restore records what it downloaded, so the next sync sends nothing.
        useRoot(false);
        Sync::RestoreResult restored = Sync::downloadToStaging(config, password, progress);
        Database fresh(Config::dbPath());
        Backup::commitRestore(restored.stagingDir, fresh);
        Sync::PushResult afterRestore = Sync::push(config, password, fresh.getAllAssetsWithFiles(),
                                                   fresh.loadSyncState(), false, progress);
        QCOMPARE(afterRestore.assets, 0);
        QCOMPARE(afterRestore.files, 0);
    }

    void accountLifecycle() {
        if (qEnvironmentVariableIsEmpty("ASSET_TRACKER_MOCK")) QSKIP("mock not running");
        mockCall("POST", "/__reset");
        useRoot(true);
        const auto progress = &TestCore::noProgress;
        const QString syncPassword = "new user sync pw";

        FirebaseConfig config = mockConfig();
        config.email = "new@example.com";
        config.password = "first-pass";

        // Creating an account signs in; a second one with the same email is refused.
        Sync::createAccount(config);
        Sync::signIn(config);
        try {
            Sync::createAccount(config);
            QFAIL("duplicate email accepted");
        } catch (const std::exception &e) {
            QVERIFY(QString(e.what()).contains("already exists"));
        }
        FirebaseConfig weak = config;
        weak.email = "weak@example.com";
        weak.password = "123";
        try {
            Sync::createAccount(weak);
            QFAIL("weak password accepted");
        } catch (const std::exception &e) {
            QVERIFY(QString(e.what()).contains("at least 6 characters"));
        }

        // Password reset is requested without signing in.
        Sync::sendPasswordReset(config, "new@example.com");
        QVERIFY(mockCall("GET", "/__dump").contains("new@example.com"));

        // A new account has nothing online, so any sync password is accepted.
        QCOMPARE(Sync::checkSyncPassword(config, syncPassword), Sync::KeyCheck::NoCloudData);

        Database db(Config::dbPath());
        const QVector<AssetRecord> records = db.getAllAssetsWithFiles();
        QCOMPARE(Sync::push(config, syncPassword, records, SyncState(), false, progress).files, 8);
        QCOMPARE(Sync::checkSyncPassword(config, syncPassword), Sync::KeyCheck::Matches);
        QCOMPARE(Sync::checkSyncPassword(config, "wrong one"), Sync::KeyCheck::DoesNotMatch);

        // Another user's data is untouched by anything this user does.
        const QString reference = m_expected.value("sync_password").toString();
        Sync::push(mockConfig(), reference, records, SyncState(), false, progress);

        Sync::changeAccountPassword(config, "second-pass");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, Sync::signIn(config));
        config.password = "second-pass";
        Sync::signIn(config);

        // Deleting online data empties this user's space but keeps the account.
        Sync::deleteAllCloudData(config, progress);
        {
            FirebaseClient client(config);
            client.authenticate();
            QVERIFY(client.listDocuments("assets").isEmpty());
            QVERIFY(client.listObjects(QString()).isEmpty());
            QVERIFY(client.getDocument("metadata", "manifest").isEmpty());
        }
        QCOMPARE(Sync::checkSyncPassword(config, syncPassword), Sync::KeyCheck::NoCloudData);

        // Deleting the account removes the login too.
        Sync::push(config, syncPassword, records, SyncState(), false, progress);
        Sync::deleteAccount(config, progress);
        try {
            Sync::signIn(config);
            QFAIL("deleted account still signs in");
        } catch (const std::exception &e) {
            QVERIFY(QString(e.what()).contains("not correct"));
        }
        Sync::createAccount(config);   // The email is free again, with nothing behind it.
        QCOMPARE(Sync::checkSyncPassword(config, syncPassword), Sync::KeyCheck::NoCloudData);

        // The reference user still has everything.
        FirebaseClient other(mockConfig());
        other.authenticate();
        QCOMPARE(other.listDocuments("assets").size(), 3);
        QCOMPARE(other.listObjects("files/").size(), 8);
    }

    void syncPushDeleteAndRestore() {
        if (qEnvironmentVariableIsEmpty("ASSET_TRACKER_MOCK")) QSKIP("mock not running");
        mockCall("POST", "/__reset");
        useRoot(true);
        const FirebaseConfig config = mockConfig();
        const QString password = m_expected.value("sync_password").toString();
        const auto progress = &TestCore::noProgress;

        QVector<AssetRecord> records;
        {
            Database db(Config::dbPath());
            records = db.getAllAssetsWithFiles();
        }

        Sync::PushResult pushed = Sync::push(config, password, records, SyncState(), false, progress);
        QVERIFY2(pushed.errors.isEmpty(), qPrintable(pushed.errors.join("; ")));
        QCOMPARE(pushed.assets, 3);
        QCOMPARE(pushed.files, 8);
        QVERIFY(pushed.cloudOnly.isEmpty());

        // A wrong password stops before anything is written.
        const Sync::PushResult wrong = Sync::push(config, "not it", records, SyncState(), false, progress);
        QVERIFY(wrong.keyMismatch);
        QCOMPARE(wrong.assets, 0);

        // An empty PC must never overwrite a populated cloud.
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 Sync::push(config, password, {}, SyncState(), false, progress));

        // Drop a photo and an asset locally: the photo goes at once, the asset is
        // only reported, and its manifest entry survives.
        QVector<AssetRecord> fewer = records;
        const AssetRecord dropped = fewer.takeLast();
        fewer[0].files.removeLast();
        const int removedFiles = records[0].files.size() - fewer[0].files.size();
        QCOMPARE(removedFiles, 1);
        pushed = Sync::push(config, password, fewer, SyncState(), false, progress);
        QCOMPARE(pushed.cloudOnly.size(), 1);
        QCOMPARE(pushed.cloudOnly.first().id, dropped.asset.id);
        QCOMPARE(pushed.cloudOnly.first().name, dropped.asset.name);

        FirebaseClient client(config);
        client.authenticate();
        QCOMPARE(client.listObjects("files/").size(), 7);
        const QByteArray key = Crypto::deriveKey(password);
        QJsonObject manifest = Crypto::decryptJson(client.getDocument("metadata", "manifest"), key);
        QVERIFY(manifest.contains(dropped.asset.id));

        // Everything is still restorable at this point.
        useRoot(false);
        Sync::RestoreResult restored = Sync::downloadToStaging(config, password, progress);
        QVERIFY2(restored.errors.isEmpty(), qPrintable(restored.errors.join("; ")));
        QCOMPARE(restored.assets, 3);
        QCOMPARE(restored.files, 7);
        {
            Database db(Config::dbPath());
            Backup::commitRestore(restored.stagingDir, db);
            QCOMPARE(db.getAllAssets().size(), 3);
            const auto tv = db.getAssetById("00001");
            QCOMPARE(*tv->valueEstimate, 1234567.89);
            const QVector<AssetFile> files = db.getAssetFiles("00001");
            QVERIFY(!files.isEmpty());
            for (const AssetFile &file : files) {
                QVERIFY(file.encrypted);
                QVERIFY(!Storage::readFile(Storage::storedPath(*tv, file), true).isEmpty());
            }
        }

        Sync::deleteFromCloud(config, password, {dropped.asset.id}, progress);
        QCOMPARE(client.listDocuments("assets").size(), 2);
        QVERIFY(client.listObjects(QString("files/%1/").arg(dropped.asset.id)).isEmpty());
        manifest = Crypto::decryptJson(client.getDocument("metadata", "manifest"), key);
        QVERIFY(!manifest.contains(dropped.asset.id));

        // Bad credentials report cleanly and do not leak the key.
        FirebaseConfig bad = config;
        bad.password = "nope";
        try {
            Sync::push(bad, password, records, SyncState(), false, progress);
            QFAIL("authentication should have failed");
        } catch (const std::exception &e) {
            QVERIFY(QString(e.what()).contains("email or password is not correct"));
            QVERIFY(!QString(e.what()).contains(config.apiKey));
        }

        // Leave a full push in place for verify_outputs.py.
        useRoot(true);
        Sync::push(config, password, records, SyncState(), false, progress);
    }

    void restoreWithWrongSyncPasswordChangesNothing() {
        if (qEnvironmentVariableIsEmpty("ASSET_TRACKER_MOCK")) QSKIP("mock not running");
        useRoot(true);
        FirebaseConfig config = mockConfig();
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
            Sync::downloadToStaging(config, "wrong sync password", &TestCore::noProgress));
        QVERIFY(strayFolders().isEmpty());
    }

    void reportsGenerate() {
        useRoot(true);
        Database db(Config::dbPath());
        const QVector<AssetRecord> records = db.getAllAssetsWithFiles();
        InsurerInfo insurer;
        insurer.company = "Acme Mutual";
        insurer.policyNumber = "PO-1";
        insurer.dateOfLoss = "May 1, 2026";

        const QString full = m_out + "/cpp_report.pdf";
        Report::generate(records, full, insurer);
        QPdfDocument document;
        QCOMPARE(document.load(full), QPdfDocument::Error::None);
        QVERIFY(document.pageCount() >= 5);
        document.close();

        const QString single = m_out + "/cpp_single.pdf";
        Report::generateSingle(records.first().asset, records.first().files, single, insurer);
        QCOMPARE(document.load(single), QPdfDocument::Error::None);
        QVERIFY(document.pageCount() >= 1);
        document.close();

        // A damaged photo prints as a placeholder rather than stopping the report.
        const AssetRecord &first = records.first();
        Storage::writeFile(Storage::storedPath(first.asset, first.files.first()),
                           Storage::protect("not an image"));
        Report::generateSingle(first.asset, first.files, m_out + "/cpp_damaged.pdf", insurer);
    }

    void csvExport() {
        Asset asset;
        asset.id = "00001";
        asset.name = "Quote \" and, comma";
        asset.valueEstimate = 1234.5;
        asset.dateAdded = "2026-01-02T03:04:05Z";
        asset.notes = "two\nlines";
        const QString path = m_out + "/cpp_export.csv";
        CsvExport::write({asset}, path);

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray data = file.readAll();
        QVERIFY(data.startsWith("\xEF\xBB\xBF" "Asset ID,Name,"));
        QVERIFY(data.contains("00001,\"Quote \"\" and, comma\",,,,,1234.50,,No,2026-01-02,"
                              "\"two\nlines\""));
    }

    void opensRealDataCopy() {
        const QString source = qEnvironmentVariable("ASSET_TRACKER_REAL_DATA");
        if (source.isEmpty()) QSKIP("no real data folder given");
        useRoot(false);
        copyTree(source, Config::dataDir());

        Database db(Config::dbPath());
        const QVector<AssetRecord> records = db.getAllAssetsWithFiles();
        int files = 0;
        for (const AssetRecord &record : records) {
            for (const AssetFile &file : record.files) {
                const QString path = Storage::storedPath(record.asset, file);
                QVERIFY2(QFile::exists(path), "database row with no file");
                QVERIFY(!Storage::readFile(path, file.encrypted).isEmpty());
                ++files;
            }
        }
        qInfo("real data copy: %lld assets, %d files all readable",
              static_cast<long long>(records.size()), files);
    }

    void uiConstructs() {
        useRoot(true);
        Database db(Config::dbPath());

        Theme::apply(*qApp);

        MainWindow window(&db);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto *list = window.findChild<AssetList *>();
        QVERIFY(list);
        QCOMPARE(list->rowCount(), 3);
        list->selectRow(0);
        QCOMPARE(list->selectedAssets().size(), 1);

        // Amounts sort as numbers, not text.
        list->sortItems(5, Qt::AscendingOrder);
        QCOMPARE(list->item(2, 1)->text(), QStringLiteral("00001"));
        list->selectRow(2);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QTest::qWait(100);

        // The preview's photo tiles exist and carry pixmaps, not placeholder text.
        const QList<QLabel *> labels = window.findChildren<QLabel *>();
        const auto tiles = std::count_if(labels.cbegin(), labels.cend(), [](const QLabel *label) {
            return label->isVisible() && !label->pixmap().isNull() && label->width() == 164;
        });
        QCOMPARE(tiles, 4);

        // Tiles in the two-column grid must not be squeezed on top of each other.
        QVector<QRect> rects;
        for (const QLabel *label : labels) {
            if (label->isVisible() && !label->pixmap().isNull() && label->width() == 164)
                rects.append(QRect(label->mapToGlobal(QPoint(0, 0)), label->size()));
        }
        for (int i = 0; i < rects.size(); ++i) {
            for (int j = i + 1; j < rects.size(); ++j)
                QVERIFY(!rects[i].intersects(rects[j]));
        }

        // Whatever no longer fits must be reachable by scrolling.
        auto *preview = window.findChild<PreviewPanel *>();
        auto *scroll = preview->findChild<QScrollArea *>();
        QVERIFY(scroll->widget()->height() > scroll->viewport()->height());
        QVERIFY(scroll->verticalScrollBar()->isVisible());
        QCOMPARE(scroll->verticalScrollBar()->maximum(),
                 scroll->widget()->height() - scroll->viewport()->height());
        window.grab().save(m_out + "/main_window.png");

        // Each dialog is shown and captured, so its look can be checked by eye.
        const auto capture = [this](QDialog &dialog, const QString &name) {
            dialog.show();
            QTest::qWait(30);
            dialog.grab().save(m_out + "/" + name + ".png");
        };
        const auto tv = db.getAssetById("00001");
        AssetForm add(&db, std::nullopt, &window);
        capture(add, "dialog_add");
        AssetForm edit(&db, tv, &window);
        capture(edit, "dialog_edit");
        CloudSetupDialog setup(CloudSetupDialog::StartAt::Welcome, &window);
        capture(setup, "dialog_setup");
        CloudSetupDialog setupAccount(CloudSetupDialog::StartAt::Account, &window);
        capture(setupAccount, "dialog_setup_service");
        PreferencesDialog preferences(&db, &window);
        capture(preferences, "dialog_preferences");
        AboutDialog about(&window);
        capture(about, "dialog_about");
        HelpDialog help(&window);
        capture(help, "dialog_help");
        UpdateHistoryDialog history(&window);
        capture(history, "dialog_history");
        InsurerInfoDialog insurer(&window);
        capture(insurer, "dialog_insurer");
        window.close();
    }
};

QTEST_MAIN(TestCore)
#include "tst_core.moc"
