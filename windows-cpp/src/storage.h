#pragma once
#include <QByteArray>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include "models.h"

// Asset files on disk. Files are protected with Windows DPAPI for the current
// user, using the same call the Python edition made, so its files still open.
namespace Storage {

QByteArray protect(const QByteArray &plain);

// Throws when data is a DPAPI blob this Windows account cannot open. Data that
// is not a blob comes back unchanged (files from before encryption was added);
// the Python edition returned the blob itself on failure, so a file from another
// account looked readable and was silently garbage.
QByteArray unprotect(const QByteArray &data);
bool isProtected(const QByteArray &data);

QByteArray readFile(const QString &path, bool encrypted);
void writeFile(const QString &path, const QByteArray &data);

// assetsRoot defaults to the live assets folder; restore passes a staging tree.
// Finds an existing "<id>_*" folder if the computed name is not there, so a slug
// that differs from the one Python computed cannot lose an asset's files.
QString assetDir(const Asset &asset, const QString &assetsRoot = QString());
QString storedPath(const Asset &asset, const AssetFile &file,
                   const QString &assetsRoot = QString());

QVector<AssetFile> importFiles(const Asset &asset, const QStringList &sources,
                               const QString &fileType);
void renameAssetDir(const Asset &oldAsset, const Asset &updated);
void deleteAssetFiles(const Asset &asset);

// Decrypts into destDir/<asset folder>/. Returns the count written; files that
// could not be decrypted are named in failures.
int exportAssetFiles(const QVector<AssetRecord> &records, const QString &destDir,
                     QStringList *failures);

// Applies EXIF orientation. maxSide 0 keeps the full size.
QImage loadImage(const QString &path, bool encrypted, int maxSide = 0);
QPixmap thumbnail(const QString &path, int size, bool encrypted);

} // namespace Storage
