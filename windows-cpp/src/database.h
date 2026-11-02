#pragma once
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

#include "models.h"

struct Totals {
    int count = 0;
    std::optional<double> purchaseTotal;
    std::optional<double> currentTotal;
};

// One SQLite connection, usable only from the thread that created it. Failed
// statements throw std::runtime_error.
class Database {
public:
    explicit Database(const QString &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // Restore replaces the database file, which Windows refuses while it is open.
    void close();
    void open();
    bool integrityOk();

    QString nextAssetId();
    void insertAsset(const Asset &asset);
    void insertAssetFile(const AssetFile &file);

    QVector<Asset> getAllAssets(const QString &search = QString(),
                                const QString &category = QString());
    std::optional<Asset> getAssetById(const QString &id);
    QVector<AssetFile> getAssetFiles(const QString &assetId);
    QVector<AssetRecord> getAllAssetsWithFiles();
    Totals getTotals();

    void updateAsset(const Asset &asset);
    void updateNotes(const QString &assetId, const QString &notes);
    void deleteAsset(const QString &assetId);
    void deleteAssetFile(qint64 fileId);

private:
    void createSchema();
    void migrate();

    QString m_path;
    QString m_connection;
};
