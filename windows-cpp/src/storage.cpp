#include "storage.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPixmapCache>
#include <QSaveFile>
#include <stdexcept>

#include <windows.h>
#include <dpapi.h>

#include "config.h"

namespace {

[[noreturn]] void fail(const QString &message) {
    throw std::runtime_error(message.toStdString());
}

// Every DPAPI blob starts with this version + provider GUID.
const QByteArray kBlobHeader = QByteArray::fromHex("01000000d08c9ddf0115d1118c7a00c04fc297eb");

} // namespace

QByteArray Storage::protect(const QByteArray &plain) {
    DATA_BLOB in;
    in.cbData = static_cast<DWORD>(plain.size());
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()));

    DATA_BLOB out = {0, nullptr};
    if (!CryptProtectData(&in, nullptr, nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &out))
        fail(QString("Windows could not encrypt the file (error %1).").arg(GetLastError()));

    const QByteArray result(reinterpret_cast<const char *>(out.pbData),
                            static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return result;
}

bool Storage::isProtected(const QByteArray &data) {
    return data.startsWith(kBlobHeader);
}

QByteArray Storage::unprotect(const QByteArray &data) {
    if (!isProtected(data)) return data;

    DATA_BLOB in;
    in.cbData = static_cast<DWORD>(data.size());
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(data.constData()));

    DATA_BLOB out = {0, nullptr};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &out))
        fail(QString("This file is locked to a different Windows account or PC (error %1).")
                 .arg(GetLastError()));

    const QByteArray result(reinterpret_cast<const char *>(out.pbData),
                            static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return result;
}

QByteArray Storage::readFile(const QString &path, bool encrypted) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        fail(QString("Could not read %1: %2").arg(path, file.errorString()));
    const QByteArray raw = file.readAll();
    return encrypted ? unprotect(raw) : raw;
}

void Storage::writeFile(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        fail(QString("Could not write %1: %2").arg(path, file.errorString()));
}

QString Storage::assetDir(const Asset &asset, const QString &assetsRoot) {
    const QString root = assetsRoot.isEmpty() ? Config::assetsDir() : assetsRoot;
    const QString computed = root + "/" + asset.dirName();
    if (QFileInfo::exists(computed)) return computed;

    const QStringList found = QDir(root).entryList({asset.id + "_*"}, QDir::Dirs);
    return found.isEmpty() ? computed : root + "/" + found.first();
}

QString Storage::storedPath(const Asset &asset, const AssetFile &file, const QString &assetsRoot) {
    return assetDir(asset, assetsRoot) + "/" + file.storedName;
}

QVector<AssetFile> Storage::importFiles(const Asset &asset, const QStringList &sources,
                                        const QString &fileType) {
    const QString destDir = assetDir(asset);
    QDir().mkpath(destDir);

    QVector<AssetFile> imported;
    for (const QString &source : sources) {
        const QFileInfo info(source);
        const QString suffix = info.suffix().isEmpty() ? QString() : "." + info.suffix();
        QString storedName = info.fileName();
        for (int counter = 1; QFileInfo::exists(destDir + "/" + storedName); ++counter)
            storedName = QString("%1_%2%3").arg(info.completeBaseName()).arg(counter).arg(suffix);

        writeFile(destDir + "/" + storedName, protect(readFile(source, false)));

        AssetFile file;
        file.assetId = asset.id;
        file.fileName = info.fileName();
        file.fileType = fileType;
        file.storedName = storedName;
        file.dateAdded = utcNow();
        file.encrypted = true;
        imported.append(file);
    }
    return imported;
}

void Storage::renameAssetDir(const Asset &oldAsset, const Asset &updated) {
    const QString oldDir = assetDir(oldAsset);
    const QString newDir = Config::assetsDir() + "/" + updated.dirName();
    if (oldDir != newDir && QFileInfo::exists(oldDir) && !QDir().rename(oldDir, newDir))
        fail(QString("Could not rename %1. Is a file in it open?").arg(oldDir));
}

void Storage::deleteAssetFiles(const Asset &asset) {
    QDir dir(assetDir(asset));
    if (dir.exists() && !dir.removeRecursively())
        fail(QString("Could not delete %1. Is a file in it open?").arg(dir.path()));
}

int Storage::exportAssetFiles(const QVector<AssetRecord> &records, const QString &destDir,
                              QStringList *failures) {
    int count = 0;
    for (const AssetRecord &record : records) {
        for (const AssetFile &file : record.files) {
            const QString source = storedPath(record.asset, file);
            if (!QFileInfo::exists(source)) continue;
            try {
                const QString folder = destDir + "/" + QFileInfo(assetDir(record.asset)).fileName();
                writeFile(folder + "/" + file.storedName, readFile(source, file.encrypted));
                ++count;
            } catch (const std::exception &e) {
                failures->append(QString("%1: %2").arg(file.fileName, e.what()));
            }
        }
    }
    return count;
}

QImage Storage::loadImage(const QString &path, bool encrypted, int maxSide) {
    QByteArray data = readFile(path, encrypted);
    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    // Phone photos are large; a scaled decode saves most of the work.
    const QSize full = reader.size();
    if (maxSide > 0 && full.isValid() && qMax(full.width(), full.height()) > maxSide)
        reader.setScaledSize(full.scaled(maxSide, maxSide, Qt::KeepAspectRatio));
    return reader.read();
}

QPixmap Storage::thumbnail(const QString &path, int size, bool encrypted) {
    if (!Config::imageExtensions().contains("." + QFileInfo(path).suffix().toLower()))
        return QPixmap();

    const QFileInfo info(path);
    const QString key = QString("%1|%2|%3").arg(path).arg(size)
        .arg(info.lastModified().toMSecsSinceEpoch());
    QPixmap pixmap;
    if (QPixmapCache::find(key, &pixmap)) return pixmap;

    try {
        pixmap = QPixmap::fromImage(loadImage(path, encrypted, size));
    } catch (const std::exception &) {
        return QPixmap();
    }
    if (!pixmap.isNull()) QPixmapCache::insert(key, pixmap);
    return pixmap;
}
