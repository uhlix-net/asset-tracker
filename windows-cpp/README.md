# Asset Tracker — C++ Edition

A C++17/Qt 6 Windows application for documenting household items for insurance. This
is the default edition from 0.1.0. The Python/PyQt6 edition at the repo root and the
Android app in `android_app/` are maintained as-is; the Android app reads what this
edition syncs and needed no changes.

It upgrades a Python install in place: same `%APPDATA%\AssetTracker` data, same
registry settings, same uninstall key, same Firebase layout.

## Dependencies

- **Qt 6.9.x** (MSVC 2022 64-bit) with the **Qt PDF** add-on. Add it with the
  Maintenance Tool under *Additional Libraries → Qt PDF*. Used to embed PDF receipts.
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

The version lives in one place, `project()` in `CMakeLists.txt`. Editing `CMakeLists.txt`
or `vcpkg.json` calls for a full rebuild: delete `build\` and run all three steps.

## Tests

```cmd
python tests\run_tests.py <repo root> build\Release\asset_tracker_tests.exe %TEMP%\at_tests
```

Use a Python with the root `requirements.txt` installed. `make_fixtures.py` generates
fixtures by running the Python edition's own code, the C++ tests read them, and
`verify_outputs.py` reads what C++ wrote back with Python. `mock_firebase.py` stands in
for Firebase, so nothing touches a real project. Tests use a private data folder and
settings file. Set `ASSET_TRACKER_REAL_DATA` to an AssetTracker folder to read-test a
temporary copy of it.

Run on the real Windows platform, not `offscreen`: that platform has no fonts, so PDFs
come out with blocks in place of text.

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
