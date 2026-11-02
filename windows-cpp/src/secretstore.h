#pragma once
#include <QString>

// Reversible protection for the small secrets Asset Tracker keeps on disk —
// the Firebase and sync passwords.
//
// Backed by Windows DPAPI (CryptProtectData), so the stored blob is readable
// only by the same Windows user on the same machine. It is not a substitute
// for a key manager: anything running as that user can undo it. It exists so
// that the registry settings, which any process running as the user can read,
// never hold a password in clear text.
//
// Both calls fail closed: on any error they return an empty string rather than
// falling back to plaintext.
namespace SecretStore {

// Plaintext -> base64-encoded protected blob. Empty in, empty out.
QString protect(const QString &plain);

// Base64-encoded protected blob -> plaintext. Empty if it cannot be read,
// which is what happens to a value copied from another machine or
// another Windows account.
QString unprotect(const QString &stored);

// False on platforms with no DPAPI, where nothing is ever written.
bool available();

} // namespace SecretStore
