#pragma once
#include <QString>

#include "firebaseclient.h"

// Where online sync settings live, and the one place that reads or writes them.
//
// The Firebase project (API key, project ID, storage bucket) is shared by every
// user: it is the app's backend, not the user's. A build can bake it in and hide
// it (ASSET_TRACKER_FIREBASE_LOCKED); otherwise it is entered once at first run.
// Each user has their own account (email and password) in that project, and their
// own sync password, which encrypts their data and is never sent anywhere.
//
// Passwords are stored only as DPAPI blobs (SecretStore), never in clear text.
namespace CloudConfig {

struct Project {
    QString apiKey;
    QString projectId;
    QString bucket;
    bool isComplete() const;
};

// True when the project is compiled in and users cannot see or change it.
bool isLocked();

Project project();
void saveProject(const Project &project);    // No effect when locked.

QString email();
QString password();
void saveAccount(const QString &email, const QString &password);

bool rememberSyncPassword();
QString syncPassword();                       // Empty unless remembered.
void saveSyncPassword(const QString &password, bool remember);

// Signed in at least once and holding a password to sign in with.
bool hasAccount();
void signOut();                               // Forgets the account and sync password.

// First-run handling: the setup prompt is shown until it is completed or declined.
bool setupPrompted();
void setSetupPrompted(bool prompted);

// The full configuration for a Firebase call, with the stored account.
FirebaseConfig firebaseConfig();

// Rewrites any password an older version saved in clear text. Runs at every start.
void migrateStoredPasswords();

} // namespace CloudConfig
