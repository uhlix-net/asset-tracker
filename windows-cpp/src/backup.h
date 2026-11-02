#pragma once
#include <QString>
#include <QVector>

#include "models.h"

class Database;

// AES-256 ZIP backups of db/ and assets/.
//
// Format 2 (this edition) stores asset files decrypted, inside the archive's own
// encryption, so a backup restores on any PC or Windows account. Format 1 backups
// from the Python edition hold DPAPI-locked files and only restore for the account
// that made them. Restore is two steps so nothing live is touched until the whole
// archive has been read and checked.
namespace Backup {

QString defaultName();

// records is a snapshot of the live database. Safe off the UI thread.
void create(const QString &password, const QString &dest, const QVector<AssetRecord> &records);

// Extracts and verifies into a staging folder and returns its path. Throws, leaving
// live data alone, on a wrong password or a damaged or unusable archive. Safe off
// the UI thread.
QString prepareRestore(const QString &password, const QString &src);

// A new staging folder beside the live data, on the same volume.
QString newStagingDir();

// Swaps a verified staging tree in, keeping the old data in a timestamped folder
// beside it (returned). Reopens db. Rolls back on failure. UI thread only.
QString commitRestore(const QString &stagingDir, Database &db);

} // namespace Backup
