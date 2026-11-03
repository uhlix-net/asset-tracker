#include "backup.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmapCache>
#include <QSet>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <zip.h>

#include "config.h"
#include "database.h"
#include "storage.h"

namespace {

const char kFormatEntry[] = "backup_format.json";
constexpr int kCurrentFormat = 2;

[[noreturn]] void fail(const QString &message) {
    throw std::runtime_error(message.toStdString());
}

QString zipError(zip_t *archive) {
    return QString::fromUtf8(zip_error_strerror(zip_get_error(archive)));
}

zip_source_t *pathSource(const QString &path, zip_error_t *error) {
    const std::wstring wide = QDir::toNativeSeparators(path).toStdWString();
    return zip_source_win32w_create(wide.c_str(), 0, -1, error);
}

// Feeds one file to libzip, read and decrypted only when libzip reaches it, so
// the whole backup is never held in memory or left decrypted on disk.
struct LazyFile {
    QString path;
    bool decrypt = false;
    QString *failure = nullptr;
    QByteArray data;
    qint64 offset = 0;
    zip_error_t error;
};

zip_int64_t lazyFileCallback(void *state, void *buffer, zip_uint64_t length, zip_source_cmd_t cmd) {
    auto *file = static_cast<LazyFile *>(state);
    switch (cmd) {
    case ZIP_SOURCE_SUPPORTS:
        return zip_source_make_command_bitmap(ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_CLOSE,
                                              ZIP_SOURCE_STAT, ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE,
                                              -1);
    case ZIP_SOURCE_OPEN:
        try {
            file->data = Storage::readFile(file->path, file->decrypt);
            file->offset = 0;
            return 0;
        } catch (const std::exception &e) {
            *file->failure = QString("%1: %2").arg(file->path, e.what());
            zip_error_set(&file->error, ZIP_ER_READ, 0);
            return -1;
        }
    case ZIP_SOURCE_READ: {
        const qint64 count = qMin<qint64>(static_cast<qint64>(length),
                                          file->data.size() - file->offset);
        std::memcpy(buffer, file->data.constData() + file->offset, static_cast<size_t>(count));
        file->offset += count;
        return count;
    }
    case ZIP_SOURCE_CLOSE:
        file->data.clear();
        return 0;
    case ZIP_SOURCE_STAT: {
        auto *st = static_cast<zip_stat_t *>(buffer);
        zip_stat_init(st);
        st->mtime = QFileInfo(file->path).lastModified().toSecsSinceEpoch();
        st->valid |= ZIP_STAT_MTIME;
        return sizeof(zip_stat_t);
    }
    case ZIP_SOURCE_ERROR:
        return zip_error_to_data(&file->error, buffer, length);
    case ZIP_SOURCE_FREE:
        zip_error_fini(&file->error);
        delete file;
        return 0;
    default:
        zip_error_set(&file->error, ZIP_ER_OPNOTSUPP, 0);
        return -1;
    }
}

void addEntry(zip_t *archive, const QString &name, zip_source_t *source, const char *password) {
    const zip_int64_t index = zip_file_add(archive, name.toUtf8().constData(), source,
                                           ZIP_FL_ENC_UTF_8);
    if (index < 0) {
        zip_source_free(source);
        fail(QString("Could not add %1 to the backup: %2").arg(name, zipError(archive)));
    }
    if (zip_set_file_compression(archive, index, ZIP_CM_DEFLATE, 0) < 0
        || zip_file_set_encryption(archive, index, ZIP_EM_AES_256, password) < 0)
        fail(QString("Could not encrypt %1: %2").arg(name, zipError(archive)));
}

void addFile(zip_t *archive, const QString &name, const QString &path, bool decrypt,
             const char *password, QString *failure) {
    auto *file = new LazyFile;
    file->path = path;
    file->decrypt = decrypt;
    file->failure = failure;
    zip_error_init(&file->error);

    zip_source_t *source = zip_source_function(archive, lazyFileCallback, file);
    if (!source) {
        zip_error_fini(&file->error);
        delete file;
        fail("Could not start the backup: " + zipError(archive));
    }
    addEntry(archive, name, source, password);
}

// Entry names come from the archive, so they must not be able to climb out of
// the staging folder.
bool isSafeEntry(const QString &name) {
    if (!name.startsWith("db/") && !name.startsWith("assets/")) return false;
    const QStringList parts = name.split('/');
    for (const QString &part : parts) {
        if (part.isEmpty() || part == "." || part == ".." || part.contains(':'))
            return false;
    }
    return true;
}

void extractEntry(zip_t *archive, zip_uint64_t index, QIODevice *out, const QString &name) {
    std::unique_ptr<zip_file_t, decltype(&zip_fclose)> file(zip_fopen_index(archive, index, 0),
                                                            zip_fclose);
    if (!file) {
        if (zip_error_code_zip(zip_get_error(archive)) == ZIP_ER_WRONGPASSWD)
            fail("Wrong backup password.");
        fail(QString("Could not read %1 from the backup: %2").arg(name, zipError(archive)));
    }

    QByteArray buffer(256 * 1024, Qt::Uninitialized);
    for (;;) {
        const zip_int64_t count = zip_fread(file.get(), buffer.data(), buffer.size());
        if (count < 0)
            fail(QString("%1 in the backup is damaged: %2")
                     .arg(name, QString::fromUtf8(zip_file_strerror(file.get()))));
        if (count == 0) return;
        if (out->write(buffer.constData(), count) != count)
            fail(QString("Could not write %1: %2").arg(name, out->errorString()));
    }
}

// Removes the staging folder unless the restore gets as far as dismiss().
class StagingGuard {
public:
    explicit StagingGuard(const QString &dir) : m_dir(dir) {}
    ~StagingGuard() { if (!m_dir.isEmpty()) QDir(m_dir).removeRecursively(); }
    void dismiss() { m_dir.clear(); }
private:
    QString m_dir;
};

} // namespace

QString Backup::defaultName() {
    return QString("assets_backup_%1.zip").arg(QDate::currentDate().toString("yyyy-MM-dd"));
}

QString Backup::newStagingDir() {
    return QString("%1_restore_%2").arg(
        Config::dataDir(), QDateTime::currentDateTime().toString("yyyyMMdd_HHmmsszzz"));
}

void Backup::create(const QString &password, const QString &dest,
                    const QVector<AssetRecord> &records) {
    QDir().mkpath(QFileInfo(dest).absolutePath());

    // Written beside the destination and renamed, so a failure never leaves a
    // partial file where an older good backup used to be.
    const QString temp = dest + ".part";
    QFile::remove(temp);

    zip_error_t error;
    zip_error_init(&error);
    zip_source_t *target = pathSource(temp, &error);
    zip_t *archive = target ? zip_open_from_source(target, ZIP_CREATE | ZIP_TRUNCATE, &error)
                            : nullptr;
    if (!archive) {
        const QString message = QString::fromUtf8(zip_error_strerror(&error));
        if (target) zip_source_free(target);
        zip_error_fini(&error);
        fail(QString("Could not create %1: %2").arg(dest, message));
    }
    zip_error_fini(&error);

    QString failure;
    try {
        const QByteArray utf8 = password.toUtf8();
        const QByteArray format = QJsonDocument(QJsonObject{
            {"format", kCurrentFormat}, {"files", "plain"},
        }).toJson(QJsonDocument::Compact);

        zip_source_t *marker = zip_source_buffer(archive, format.constData(),
                                                 static_cast<zip_uint64_t>(format.size()), 0);
        if (!marker) fail("Could not start the backup: " + zipError(archive));
        addEntry(archive, kFormatEntry, marker, utf8.constData());

        QSet<QString> encrypted;
        for (const AssetRecord &record : records) {
            for (const AssetFile &file : record.files) {
                if (file.encrypted)
                    encrypted.insert(QFileInfo(Storage::storedPath(record.asset, file))
                                         .absoluteFilePath());
            }
        }

        const QDir base(Config::dataDir());
        for (const QString &root : {Config::dbDir(), Config::assetsDir()}) {
            QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = QFileInfo(it.next()).absoluteFilePath();
                addFile(archive, base.relativeFilePath(path), path, encrypted.contains(path),
                        utf8.constData(), &failure);
            }
        }

        // The compression, encryption and file reads all happen here.
        if (zip_close(archive) < 0)
            fail(failure.isEmpty() ? "Could not write the backup: " + zipError(archive) : failure);
    } catch (...) {
        zip_discard(archive);
        QFile::remove(temp);
        throw;
    }

    QFile::remove(dest);
    if (!QFile::rename(temp, dest))
        fail(QString("Could not write %1.").arg(dest));
}

QString Backup::prepareRestore(const QString &password, const QString &src) {
    zip_error_t error;
    zip_error_init(&error);
    zip_source_t *source = pathSource(src, &error);
    zip_t *raw = source ? zip_open_from_source(source, ZIP_RDONLY, &error) : nullptr;
    if (!raw) {
        const QString message = QString::fromUtf8(zip_error_strerror(&error));
        if (source) zip_source_free(source);
        zip_error_fini(&error);
        fail(QString("Could not open the backup: %1").arg(message));
    }
    zip_error_fini(&error);
    std::unique_ptr<zip_t, decltype(&zip_discard)> archive(raw, zip_discard);
    zip_set_default_password(raw, password.toUtf8().constData());

    const QString staging = newStagingDir();
    StagingGuard guard(staging);
    QDir().mkpath(staging);

    int format = 1;
    const zip_int64_t count = zip_get_num_entries(raw, 0);
    for (zip_int64_t i = 0; i < count; ++i) {
        const zip_uint64_t index = static_cast<zip_uint64_t>(i);
        const QString name = QString::fromUtf8(zip_get_name(raw, index, ZIP_FL_ENC_GUESS))
                                 .replace('\\', '/');
        if (name.endsWith('/')) continue;

        if (name == kFormatEntry) {
            QFile marker(staging + "/" + kFormatEntry);
            if (!marker.open(QIODevice::WriteOnly)) fail(marker.errorString());
            extractEntry(raw, index, &marker, name);
            marker.close();
            if (!marker.open(QIODevice::ReadOnly)) fail(marker.errorString());
            format = QJsonDocument::fromJson(marker.readAll()).object().value("format").toInt(1);
            marker.close();
            marker.remove();
            continue;
        }
        if (!isSafeEntry(name)) continue;

        const QString path = staging + "/" + name;
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile out(path);
        if (!out.open(QIODevice::WriteOnly))
            fail(QString("Could not write %1: %2").arg(path, out.errorString()));
        extractEntry(raw, index, &out, name);
    }

    if (format > kCurrentFormat)
        fail("This backup was made by a newer version of Asset Tracker.");
    if (!QFileInfo::exists(staging + "/db/tracker.db"))
        fail("This file is not an Asset Tracker backup.");

    const QString assetsRoot = staging + "/assets";
    QStringList locked;
    {
        Database db(staging + "/db/tracker.db");
        if (!db.integrityOk())
            fail("The database in this backup is damaged.");

        const QVector<AssetRecord> records = db.getAllAssetsWithFiles();
        for (const AssetRecord &record : records) {
            for (const AssetFile &file : record.files) {
                if (!file.encrypted) continue;
                const QString path = Storage::storedPath(record.asset, file, assetsRoot);
                if (!QFileInfo::exists(path)) continue;
                if (format >= 2) {
                    Storage::writeFile(path, Storage::protect(Storage::readFile(path, false)));
                    continue;
                }
                try {
                    Storage::readFile(path, true);
                } catch (const std::exception &) {
                    locked << file.fileName;
                }
            }
        }
    }

    if (!locked.isEmpty()) {
        fail(QString("%1 file(s) in this backup are locked to the Windows account that made it, "
                     "so they cannot be opened here. Your current data has not been changed. "
                     "Sign in to the original account to restore this backup, or use "
                     "Restore from Cloud.").arg(locked.size()));
    }

    guard.dismiss();
    return staging;
}

QString Backup::commitRestore(const QString &stagingDir, Database &db) {
    const QString live = Config::dataDir();
    const QString aside = QString("%1_pre_restore_%2").arg(
        live, QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    db.close();
    QDir dir;
    const bool hadData = QFileInfo::exists(live);
    if (hadData && !dir.rename(live, aside)) {
        db.open();
        fail("Could not move the current data aside. Close anything using it and try again.");
    }
    if (!dir.rename(stagingDir, live)) {
        if (hadData) dir.rename(aside, live);
        db.open();
        fail("Could not move the restored data into place. Your data has not been changed.");
    }

    QPixmapCache::clear();
    db.open();
    return hadData ? aside : QString();
}
