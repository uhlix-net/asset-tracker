# Asset Tracker release notes

## 0.2.0 — Online Accounts and Faster Sync

### New
- **Create your online sync account from inside the app.** A short setup runs the first
  time Asset Tracker starts: connect to the online service, create an account or sign in,
  and choose your sync password. You can skip it and set it up later from
  **File → Preferences**.
- **Sign in on a new PC** with your existing account, with a link to reset a forgotten
  password.
- **File → Preferences** to manage online sync:
  - change your account password, or have a reset link emailed to you
  - remember or change your sync password (changing it uploads everything again under the
    new password)
  - sign out, or switch to another account
- **Delete Online Data** removes every item, photo and receipt stored online but keeps
  your account. **Delete Account** removes the online data and the account itself. Both
  warn that this is permanent and ask you to type DELETE. Neither touches the data on
  your PC.
- **Sync Now** (Ctrl+Shift+S) and **Restore from Cloud** are now directly in the File menu.

### Fixed
- Asset Tracker no longer crashes when you sync on PCs with an older Microsoft Visual C++
  runtime. The installer now includes the runtime the app needs.

### Improved
- **Sync only sends what changed.** After the first sync, only new or edited items and new
  photos are uploaded. Anything missing from your online copy is sent again automatically.
- After **Restore from Cloud**, the next sync has nothing to upload.
- Clearer messages when signing in fails, such as a wrong password, an account that
  already exists, or a password that is too short.
- If Asset Tracker ever crashes, it saves a crash report in
  `%LOCALAPPDATA%\AssetTracker` to help find the cause.

### For administrators
- The online service connection (Firebase API key, project ID and storage bucket) can be
  built into the app and hidden from users. See `README.md`, "Online sync backend".

## 0.1.0 — Initial C++ Release

- Rewritten in C++ and Qt 6 for speed. Existing data, settings and cloud copies carry over
  unchanged.
- A cleaner, more consistent look throughout.
- Firebase and sync passwords are stored encrypted for your Windows account, and any
  saved in plain text are converted at first start.
- Backups restore on any PC or Windows account.
- A wrong backup password or damaged archive is caught before your data is touched, and
  restoring no longer needs a restart.
- New: Restore from Cloud, for recovering everything on a new PC.
- Sync removes deleted photos from the cloud, and asks before deleting assets that are no
  longer on this PC.
- Sync stops if the sync password does not match the cloud copy.
- Opening a receipt shows the receipt rather than the encrypted file.
- Purchase price and current value columns sort by amount.
