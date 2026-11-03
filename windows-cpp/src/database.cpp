#include "database.h"

#include <QAtomicInt>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>
#include <stdexcept>

namespace {

[[noreturn]] void fail(const QSqlError &err) {
    throw std::runtime_error(err.text().toStdString());
}

void run(QSqlQuery &q) {
    if (!q.exec()) fail(q.lastError());
}

void run(QSqlDatabase db, const QString &sql) {
    QSqlQuery q(db);
    if (!q.exec(sql)) fail(q.lastError());
}

// For NOT NULL columns: a default-constructed QString binds as SQL NULL.
QVariant notNull(const QString &text) {
    return text.isNull() ? QVariant(QStringLiteral("")) : QVariant(text);
}

QVariant nullable(const QString &text) {
    return text.isEmpty() ? QVariant(QMetaType::fromType<QString>()) : QVariant(text);
}

QVariant nullable(std::optional<double> value) {
    return value ? QVariant(*value) : QVariant(QMetaType::fromType<double>());
}

std::optional<double> optionalDouble(const QVariant &v) {
    if (v.isNull()) return std::nullopt;
    return v.toDouble();
}

Asset rowToAsset(const QSqlQuery &q) {
    Asset a;
    a.id           = q.value("id").toString();
    a.name         = q.value("name").toString();
    a.category     = q.value("category").toString();
    a.datePurchase = q.value("date_purchase").toString();
    a.valueEstimate = optionalDouble(q.value("value_estimate"));
    a.currentValue  = optionalDouble(q.value("current_value"));
    a.serialNumber = q.value("serial_number").toString();
    a.modelNumber  = q.value("model_number").toString();
    a.hasReceipt   = q.value("has_receipt").toInt() != 0;
    a.dateAdded    = q.value("date_added").toString();
    a.notes        = q.value("notes").toString();
    return a;
}

AssetFile rowToFile(const QSqlQuery &q) {
    AssetFile f;
    f.id         = q.value("id").toLongLong();
    f.assetId    = q.value("asset_id").toString();
    f.fileName   = q.value("file_name").toString();
    f.fileType   = q.value("file_type").toString();
    f.storedName = q.value("stored_name").toString();
    f.dateAdded  = q.value("date_added").toString();
    f.encrypted  = q.value("encrypted").toInt() != 0;
    return f;
}

QAtomicInt g_nextConnection;

} // namespace

Database::Database(const QString &path)
    : m_path(path)
    , m_connection(QString("assets_%1").arg(g_nextConnection.fetchAndAddRelaxed(1)))
{
    QSqlDatabase::addDatabase("QSQLITE", m_connection);
    open();
}

Database::~Database() {
    close();
    QSqlDatabase::removeDatabase(m_connection);
}

void Database::open() {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    db.setDatabaseName(m_path);
    if (!db.open()) fail(db.lastError());
    run(db, "PRAGMA foreign_keys = ON");
    createSchema();
    migrate();
}

void Database::close() {
    QSqlDatabase::database(m_connection, false).close();
}

void Database::createSchema() {
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    run(db, R"(
        CREATE TABLE IF NOT EXISTS assets (
            id              TEXT PRIMARY KEY,
            name            TEXT NOT NULL,
            category        TEXT NOT NULL DEFAULT '',
            date_purchase   TEXT,
            value_estimate  REAL,
            current_value   REAL,
            serial_number   TEXT NOT NULL DEFAULT '',
            model_number    TEXT NOT NULL DEFAULT '',
            has_receipt     INTEGER NOT NULL DEFAULT 0,
            date_added      TEXT NOT NULL,
            notes           TEXT NOT NULL DEFAULT ''
        ))");
    run(db, R"(
        CREATE TABLE IF NOT EXISTS asset_files (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            asset_id    TEXT NOT NULL REFERENCES assets(id) ON DELETE CASCADE,
            file_name   TEXT NOT NULL,
            file_type   TEXT NOT NULL,
            stored_name TEXT NOT NULL,
            date_added  TEXT NOT NULL,
            encrypted   INTEGER NOT NULL DEFAULT 0
        ))");
    run(db, "CREATE INDEX IF NOT EXISTS idx_af_asset_id ON asset_files(asset_id)");
    run(db, R"(
        CREATE TABLE IF NOT EXISTS sync_state (
            key         TEXT PRIMARY KEY,
            fingerprint TEXT NOT NULL
        ))");
}

// Adds columns introduced after the first release to existing databases.
void Database::migrate() {
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    const QSqlRecord assets = db.record("assets");
    const struct { const char *column; const char *sql; } migrations[] = {
        {"category",      "ALTER TABLE assets ADD COLUMN category TEXT NOT NULL DEFAULT ''"},
        {"current_value", "ALTER TABLE assets ADD COLUMN current_value REAL"},
        {"serial_number", "ALTER TABLE assets ADD COLUMN serial_number TEXT NOT NULL DEFAULT ''"},
        {"model_number",  "ALTER TABLE assets ADD COLUMN model_number TEXT NOT NULL DEFAULT ''"},
    };
    for (const auto &m : migrations) {
        if (!assets.contains(m.column))
            run(db, m.sql);
    }
    if (!db.record("asset_files").contains("encrypted"))
        run(db, "ALTER TABLE asset_files ADD COLUMN encrypted INTEGER NOT NULL DEFAULT 0");
}

bool Database::integrityOk() {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    return q.exec("PRAGMA integrity_check") && q.next() && q.value(0).toString() == "ok";
}

QString Database::nextAssetId() {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    if (!q.exec("SELECT MAX(CAST(id AS INTEGER)) FROM assets")) fail(q.lastError());
    const int next = q.next() ? q.value(0).toInt() + 1 : 1;
    return QString("%1").arg(next, 5, 10, QChar('0'));
}

void Database::insertAsset(const Asset &asset) {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("INSERT INTO assets (id, name, category, date_purchase, value_estimate, "
              "current_value, serial_number, model_number, has_receipt, date_added, notes) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(asset.id);
    q.addBindValue(notNull(asset.name));
    q.addBindValue(notNull(asset.category));
    q.addBindValue(nullable(asset.datePurchase));
    q.addBindValue(nullable(asset.valueEstimate));
    q.addBindValue(nullable(asset.currentValue));
    q.addBindValue(notNull(asset.serialNumber));
    q.addBindValue(notNull(asset.modelNumber));
    q.addBindValue(asset.hasReceipt ? 1 : 0);
    q.addBindValue(asset.dateAdded.isEmpty() ? utcNow() : asset.dateAdded);
    q.addBindValue(notNull(asset.notes));
    run(q);
}

void Database::insertAssetFile(const AssetFile &file) {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("INSERT INTO asset_files "
              "(asset_id, file_name, file_type, stored_name, date_added, encrypted) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(file.assetId);
    q.addBindValue(file.fileName);
    q.addBindValue(file.fileType);
    q.addBindValue(file.storedName);
    q.addBindValue(file.dateAdded.isEmpty() ? utcNow() : file.dateAdded);
    q.addBindValue(file.encrypted ? 1 : 0);
    run(q);
}

QVector<Asset> Database::getAllAssets(const QString &search, const QString &category) {
    QStringList clauses;
    if (!search.isEmpty()) clauses << "name LIKE ?";
    if (!category.isEmpty()) clauses << "category = ?";
    const QString where = clauses.isEmpty() ? QString() : " WHERE " + clauses.join(" AND ");

    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.setForwardOnly(true);
    q.prepare("SELECT * FROM assets" + where + " ORDER BY category, name");
    if (!search.isEmpty()) q.addBindValue("%" + search + "%");
    if (!category.isEmpty()) q.addBindValue(category);
    run(q);

    QVector<Asset> assets;
    while (q.next())
        assets.append(rowToAsset(q));
    return assets;
}

std::optional<Asset> Database::getAssetById(const QString &id) {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT * FROM assets WHERE id = ?");
    q.addBindValue(id);
    run(q);
    if (!q.next()) return std::nullopt;
    return rowToAsset(q);
}

QVector<AssetFile> Database::getAssetFiles(const QString &assetId) {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.setForwardOnly(true);
    q.prepare("SELECT * FROM asset_files WHERE asset_id = ? ORDER BY file_type, id");
    q.addBindValue(assetId);
    run(q);

    QVector<AssetFile> files;
    while (q.next())
        files.append(rowToFile(q));
    return files;
}

QVector<AssetRecord> Database::getAllAssetsWithFiles() {
    QVector<AssetRecord> records;
    const QVector<Asset> assets = getAllAssets();
    for (const Asset &asset : assets)
        records.append({asset, getAssetFiles(asset.id)});
    return records;
}

Totals Database::getTotals() {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    if (!q.exec("SELECT COUNT(*), SUM(value_estimate), SUM(current_value) FROM assets"))
        fail(q.lastError());
    Totals totals;
    if (q.next()) {
        totals.count = q.value(0).toInt();
        totals.purchaseTotal = optionalDouble(q.value(1));
        totals.currentTotal = optionalDouble(q.value(2));
    }
    return totals;
}

void Database::updateAsset(const Asset &asset) {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("UPDATE assets SET name=?, category=?, date_purchase=?, value_estimate=?, "
              "current_value=?, serial_number=?, model_number=?, has_receipt=?, notes=? "
              "WHERE id=?");
    q.addBindValue(notNull(asset.name));
    q.addBindValue(notNull(asset.category));
    q.addBindValue(nullable(asset.datePurchase));
    q.addBindValue(nullable(asset.valueEstimate));
    q.addBindValue(nullable(asset.currentValue));
    q.addBindValue(notNull(asset.serialNumber));
    q.addBindValue(notNull(asset.modelNumber));
    q.addBindValue(asset.hasReceipt ? 1 : 0);
    q.addBindValue(notNull(asset.notes));
    q.addBindValue(asset.id);
    run(q);
}

void Database::updateNotes(const QString &assetId, const QString &notes) {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("UPDATE assets SET notes = ? WHERE id = ?");
    q.addBindValue(notNull(notes));
    q.addBindValue(assetId);
    run(q);
}

void Database::deleteAsset(const QString &assetId) {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("DELETE FROM assets WHERE id = ?");
    q.addBindValue(assetId);
    run(q);
}

namespace {
const char kTargetKey[] = "@target";
}

SyncState Database::loadSyncState() {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.setForwardOnly(true);
    if (!q.exec("SELECT key, fingerprint FROM sync_state")) fail(q.lastError());
    SyncState state;
    while (q.next()) {
        const QString key = q.value(0).toString();
        if (key == kTargetKey) state.target = q.value(1).toString();
        else state.fingerprints.insert(key, q.value(1).toString());
    }
    return state;
}

void Database::saveSyncState(const SyncState &state) {
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    if (!db.transaction()) fail(db.lastError());
    try {
        run(db, "DELETE FROM sync_state");
        QSqlQuery q(db);
        q.prepare("INSERT INTO sync_state (key, fingerprint) VALUES (?, ?)");
        const auto insert = [&q](const QString &key, const QString &value) {
            q.addBindValue(key);
            q.addBindValue(value);
            run(q);
        };
        if (!state.target.isEmpty()) insert(kTargetKey, state.target);
        for (auto it = state.fingerprints.cbegin(); it != state.fingerprints.cend(); ++it)
            insert(it.key(), it.value());
    } catch (...) {
        db.rollback();
        throw;
    }
    if (!db.commit()) fail(db.lastError());
}

void Database::deleteAssetFile(qint64 fileId) {
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("DELETE FROM asset_files WHERE id = ?");
    q.addBindValue(fileId);
    run(q);
}
