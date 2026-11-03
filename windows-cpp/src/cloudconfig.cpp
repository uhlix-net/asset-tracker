#include "cloudconfig.h"

#include "config.h"
#include "secretstore.h"

#ifndef ASSET_TRACKER_FIREBASE_LOCKED
#define ASSET_TRACKER_FIREBASE_LOCKED 0
#endif
#ifndef ASSET_TRACKER_FIREBASE_API_KEY
#define ASSET_TRACKER_FIREBASE_API_KEY ""
#endif
#ifndef ASSET_TRACKER_FIREBASE_PROJECT_ID
#define ASSET_TRACKER_FIREBASE_PROJECT_ID ""
#endif
#ifndef ASSET_TRACKER_FIREBASE_BUCKET
#define ASSET_TRACKER_FIREBASE_BUCKET ""
#endif

namespace {

const char kApiKey[] = "firebase/api_key";
const char kProjectId[] = "firebase/project_id";
const char kBucket[] = "firebase/bucket";
const char kEmail[] = "firebase/email";
const char kPassword[] = "firebase/password";
const char kSyncPassword[] = "firebase/sync_password";
const char kRemember[] = "firebase/remember_sync_password";
const char kPrompted[] = "firebase/setup_prompted";
const char kEncSuffix[] = "_enc";

QString readSecret(QSettings &settings, const QString &key) {
    return SecretStore::unprotect(settings.value(key + kEncSuffix).toString());
}

void writeSecret(QSettings &settings, const QString &key, const QString &value) {
    if (value.isEmpty()) settings.remove(key + kEncSuffix);
    else settings.setValue(key + kEncSuffix, SecretStore::protect(value));
    settings.remove(key);
}

void migrateSecret(QSettings &settings, const QString &key) {
    if (!settings.contains(key)) return;
    const QString plain = settings.value(key).toString();
    // If DPAPI failed here, leave the value for the next start rather than lose it.
    const QString sealed = SecretStore::protect(plain);
    if (plain.isEmpty() || !sealed.isEmpty()) {
        if (!plain.isEmpty()) settings.setValue(key + kEncSuffix, sealed);
        settings.remove(key);
    }
}

} // namespace

bool CloudConfig::Project::isComplete() const {
    return !apiKey.isEmpty() && !projectId.isEmpty() && !bucket.isEmpty();
}

bool CloudConfig::isLocked() {
    return ASSET_TRACKER_FIREBASE_LOCKED && !Config::isTestRun();
}

CloudConfig::Project CloudConfig::project() {
    if (isLocked()) {
        return {QStringLiteral(ASSET_TRACKER_FIREBASE_API_KEY),
                QStringLiteral(ASSET_TRACKER_FIREBASE_PROJECT_ID),
                QStringLiteral(ASSET_TRACKER_FIREBASE_BUCKET)};
    }
    const auto settings = Config::openSettings();
    Project project{settings->value(kApiKey).toString(), settings->value(kProjectId).toString(),
                    settings->value(kBucket).toString()};
    // A build can suggest a project without locking it; saved values win.
    if (project.apiKey.isEmpty()) project.apiKey = QStringLiteral(ASSET_TRACKER_FIREBASE_API_KEY);
    if (project.projectId.isEmpty())
        project.projectId = QStringLiteral(ASSET_TRACKER_FIREBASE_PROJECT_ID);
    if (project.bucket.isEmpty()) project.bucket = QStringLiteral(ASSET_TRACKER_FIREBASE_BUCKET);
    return project;
}

void CloudConfig::saveProject(const Project &project) {
    if (isLocked()) return;
    const auto settings = Config::openSettings();
    settings->setValue(kApiKey, project.apiKey);
    settings->setValue(kProjectId, project.projectId);
    settings->setValue(kBucket, project.bucket);
}

QString CloudConfig::email() {
    return Config::openSettings()->value(kEmail).toString();
}

QString CloudConfig::password() {
    return readSecret(*Config::openSettings(), kPassword);
}

void CloudConfig::saveAccount(const QString &email, const QString &password) {
    const auto settings = Config::openSettings();
    settings->setValue(kEmail, email);
    writeSecret(*settings, kPassword, password);
}

bool CloudConfig::rememberSyncPassword() {
    // Older versions stored this as the string "True"; QVariant reads that as true too.
    return Config::openSettings()->value(kRemember).toBool();
}

QString CloudConfig::syncPassword() {
    const auto settings = Config::openSettings();
    return settings->value(kRemember).toBool() ? readSecret(*settings, kSyncPassword) : QString();
}

void CloudConfig::saveSyncPassword(const QString &password, bool remember) {
    const auto settings = Config::openSettings();
    settings->setValue(kRemember, remember);
    writeSecret(*settings, kSyncPassword, remember ? password : QString());
}

bool CloudConfig::hasAccount() {
    return !email().isEmpty() && !password().isEmpty();
}

void CloudConfig::signOut() {
    const auto settings = Config::openSettings();
    settings->remove(kEmail);
    writeSecret(*settings, kPassword, QString());
    writeSecret(*settings, kSyncPassword, QString());
    settings->setValue(kRemember, false);
}

bool CloudConfig::setupPrompted() {
    return Config::openSettings()->value(kPrompted).toBool();
}

void CloudConfig::setSetupPrompted(bool prompted) {
    Config::openSettings()->setValue(kPrompted, prompted);
}

FirebaseConfig CloudConfig::firebaseConfig() {
    const Project p = project();
    FirebaseConfig config;
    config.apiKey = p.apiKey;
    config.projectId = p.projectId;
    config.bucket = p.bucket;
    config.email = email();
    config.password = password();
    return config;
}

void CloudConfig::migrateStoredPasswords() {
    const auto settings = Config::openSettings();
    migrateSecret(*settings, kPassword);
    migrateSecret(*settings, kSyncPassword);
}
