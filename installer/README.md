# Building the Asset Tracker Installer

## Prerequisites (Windows only)

Install **NSIS 3.x** (Nullsoft Scriptable Install System):
- Download from https://nsis.sourceforge.io/Download
- Run the installer — default options are fine
- NSIS is free and open source

## How to build

From Windows (not WSL), double-click **`build_installer.bat`** inside this folder.

It will:
1. Locate `makensis.exe` automatically
2. Compile `installer.nsi`
3. Produce `installer\AssetTracker_Setup.exe`

The script file must be run from the `installer\` subdirectory of the project.
Both `installer\` and the project root (containing `main.py`, `asset_tracker\`) must be present.

## What the installer does

1. **Checks for Python 3.10+** in the Windows registry and PATH
2. **If Python is not found** — shows a dialog asking to download Python 3.12 (~27 MB from python.org)
   - Automatically downloads and silently installs Python with `PrependPath=1`
   - Continues with app installation after Python is ready
3. **Asks for an installation folder** (default: `C:\Program Files\AssetTracker`)
4. **Copies application files** to the chosen folder
5. **Creates a Python virtual environment** inside the installation folder
6. **Installs dependencies** (`PyQt6`, `Pillow`, `reportlab`, `pyzipper`) via pip
7. **Creates shortcuts** on the Desktop and in the Start Menu
8. **Registers** the app in Add/Remove Programs (Windows Settings → Apps)

## What the uninstaller does

- Removes all installed files and the virtual environment
- Removes Start Menu and Desktop shortcuts
- Removes the Add/Remove Programs entry
- The user's data (`%APPDATA%\AssetTracker\`) is **preserved** — uninstalling does not delete assets or the database

## Notes

- The installer requires **administrator privileges** (to write to Program Files and the registry)
- Internet access is only needed if Python is not already installed
- The `AssetTracker_Setup.exe` output file is excluded from git (see `.gitignore`)
