# Asset Tracker

A C++17/Qt 6 Windows application for documenting household items for insurance. The
Android app in `android_app/` reads what this app syncs to Firebase.

Installing over an older version upgrades it in place: user data stays in
`%APPDATA%\AssetTracker`, settings in `HKCU\Software\AssetTracker`.

## Dependencies

- **Qt 6.9.x** (MSVC 2022 64-bit) with the **Qt PDF** add-on. Add it with the
  Maintenance Tool under *Additional Libraries → Qt PDF*. It's used to embed PDF receipts.
- **OpenSSL 3.x** (sync encryption) and **libzip** (AES backups), via vcpkg in
  manifest mode: `vcpkg.json` installs them during configure.
- **CMake 4.2+**, **Visual Studio 2026** with the C++ workload, **NSIS 3.x**.

## Build

From `windows-cpp`:

```cmd
cmake -B build -G "Visual Studio 18 2026" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DQt6_DIR=C:/Qt/6.5.3/msvc2019_64/6.9.3/msvc2022_64/lib/cmake/Qt6 ^
  -DASSET_TRACKER_TESTS=ON
cmake --build build --config Release
cd installer && "C:\Program Files (x86)\NSIS\makensis.exe" setup.nsi
```

`--config Release` is required, or you silently get a Debug build. Output is
`installer\Output\AssetTracker_Setup.exe`.

The version is set in one place, `project()` in `CMakeLists.txt`. Editing `CMakeLists.txt`
or `vcpkg.json` calls for a full rebuild: delete `build\` and run all three steps.

## Online sync backend

Every user has an account in one shared Firebase project, which is the app's backend.
By default the project's API key, project ID and storage bucket are entered at first
start (and can be changed under File → Preferences). For a production build, bake
them in and hide them from users:

```cmd
cmake -B build ... ^
  -DASSET_TRACKER_FIREBASE_API_KEY=<web API key> ^
  -DASSET_TRACKER_FIREBASE_PROJECT_ID=<project id> ^
  -DASSET_TRACKER_FIREBASE_BUCKET=<project id>.firebasestorage.app ^
  -DASSET_TRACKER_FIREBASE_LOCKED=ON
```

Without `LOCKED`, the same three values are only suggestions that users can change.
The project needs Email/Password sign-in turned on, with sign-up allowed, and the
per-user Firestore and Storage rules (`users/{uid}/...`). Sync only sends what changed:
`SyncState` fingerprints are kept in the `sync_state` table and checked against what
the cloud actually holds.

## Look and feel

All colours, fonts and the stylesheet live in `src/theme.cpp`; icons are drawn in
`src/icons.cpp`. Widgets choose a style by object name (`primary`, `section`, `hint`,
...), so don't add per-widget `setStyleSheet` colours.

## Tests

```cmd
py -m venv %TEMP%\at_venv
%TEMP%\at_venv\Scripts\pip install -r tests\requirements.txt
%TEMP%\at_venv\Scripts\python tests\run_tests.py build\Release\asset_tracker_tests.exe %TEMP%\at_tests
```

`make_fixtures.py` writes sample data in the formats older versions produced: the
database, DPAPI-encrypted files, backups and sync blobs. The C++ tests read it, and
`verify_outputs.py` independently checks what C++ wrote. `mock_firebase.py` stands in for
Firebase, so nothing touches a real project. Tests use a private data folder and settings
file. Set `ASSET_TRACKER_REAL_DATA` to an AssetTracker folder to read-test a temporary
copy of it.

The tests run on the real Windows platform, not `offscreen`. That platform has no fonts,
so PDFs come out with blocks in place of text. A few windows flash on screen.

## Compatibility rules

- `crypto.cpp` must stay in step with `android_app/lib/services/crypto_service.dart`: the
  salt, iteration count and blob layout. Sync records are all strings, as `Asset.fromMap`
  expects.
- Sync must never delete an asset just because it is missing locally. `Sync::push` only
  reports those; the user confirms. The cloud may be the only surviving copy.
- Bind text to NOT NULL columns through `notNull()`: a default-constructed `QString`
  binds as SQL NULL.
- Asset folders are `<id>_<slug>`. Go through `Storage::assetDir()`, which falls back to
  a `<id>_*` match, rather than building the path.
- Restore is two steps: `prepareRestore`/`downloadToStaging` verify everything off to the
  side, then `commitRestore` swaps. Nothing live changes before that swap.
- Passwords go through `SecretStore` (DPAPI) as `*_enc` settings, never in clear text.
