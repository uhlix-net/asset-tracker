from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QLabel, QDialogButtonBox,
    QScrollArea, QWidget, QFrame,
)
from PyQt6.QtCore import Qt
from ..config import APP_NAME

# Each entry: (version, title, [bullet points])
HISTORY = [
    (
        "1.1.7",
        "Bug fix — Android v1 embedding error from transitive dependency",
        [
            "Removed unused packages cached_network_image and flutter_spinkit "
            "from the Android app: cached_network_image pulled in sqflite via "
            "flutter_cache_manager, and sqflite retains PluginRegistry.Registrar "
            "which triggers Flutter 3.22's deleted-v1-embedding build check. "
            "Neither package was used in the app code.",
        ],
    ),
    (
        "1.1.6",
        "Bug fix — Android build: missing resources and manifest corrections",
        [
            "Added required Android resource files missing from the initial "
            "Flutter project structure: mipmap launcher icons (all densities), "
            "styles.xml (light + night), and launch_background.xml",
            "Fixed AndroidManifest.xml: removed deprecated flutterEmbedding "
            "meta-data that caused the v1 embedding error; added correct "
            "NormalTheme meta-data inside the activity",
            "Set explicit minSdk 21 in build.gradle (required by Firebase)",
            "Updated Gradle plugin versions to match Flutter 3.22",
        ],
    ),
    (
        "1.1.5",
        "Bug fix — GitHub Actions Android build workflow",
        [
            "Fixed a workflow error: secrets context cannot be used in "
            "step if conditions; moved the check inside the run script "
            "using a bash conditional instead",
        ],
    ),
    (
        "1.1.4",
        "Firebase cloud sync and Android companion app",
        [
            "File → Sync to Cloud (Firebase): all assets and files are "
            "AES-256-GCM encrypted with a sync password before upload — "
            "Google only ever stores ciphertext",
            "PBKDF2-HMAC-SHA256 (600 000 iterations) derives the sync key "
            "from the password — the same algorithm runs on Android",
            "Android companion app: sign in with Firebase credentials + "
            "sync password to browse your full asset inventory, view photos "
            "and receipts, and see all asset details",
            "GitHub Actions workflow automatically builds the Android APK "
            "on every push to main (see FIREBASE_SETUP.md for instructions)",
        ],
    ),
    (
        "1.1.3",
        "PDF report: TOC spacing, borderless photos, embedded receipts",
        [
            "Added whitespace below the Table of Contents heading rule "
            "for improved readability",
            "Removed borders from asset photo grid — images now display "
            "without surrounding lines",
            "Receipt images are now embedded inline in the report instead "
            "of listing 'Yes/No'; PDF receipts are rendered page-by-page "
            "and inserted directly into the document",
        ],
    ),
    (
        "1.1.2",
        "DPAPI encryption for photos and receipts; export and help",
        [
            "All uploaded photos and receipt files are now encrypted on disk "
            "using Windows DPAPI (tied to your Windows user account — "
            "no password required, decryption is automatic)",
            "File → Export Asset Files: decrypts and copies all files to a "
            "folder you choose, for attaching to emails or claim submissions",
            "Encryption reminder note added below the Add Photos button in "
            "the Add Asset and Edit Asset dialogs",
            "Help → AssetTracker Help: detailed guide to DPAPI encryption, "
            "what it protects, and how to obtain unencrypted file copies",
        ],
    ),
    (
        "1.1.1",
        "App icon, insurer info on reports, single-asset PDF",
        [
            "App icon (navy \"AT\" logo) now appears in the taskbar, "
            "window title bar, Start Menu shortcut, and Desktop shortcut",
            "Insurer details dialog (insurance company, policy number, "
            "claim number, date of loss) shown before generating the full "
            "report — details appear on the cover page and are remembered "
            "between sessions",
            "\"Print This Asset\" button in the asset details panel generates "
            "a one-page PDF record for the selected asset, including photos "
            "and receipt, with insurer info pre-filled",
        ],
    ),
    (
        "1.1.0",
        "UI improvements — icons, thumbnails, sort indicators, empty state",
        [
            "Toolbar buttons now show icons (Add, Edit, Delete, Duplicate, "
            "Backup, Print Report)",
            "Asset table shows a 40×40 photo thumbnail in the first column "
            "for quick visual identification",
            "Column sort indicators (▲/▼) now appear on the active sort "
            "column in the asset table",
            "Empty-state message shown when no assets exist: "
            "\"No assets yet — click Add Asset to get started\"",
            "Installer version now reliably matches the app version in "
            "Windows Settings → Apps → Installed Apps",
        ],
    ),
    (
        "1.0.11",
        "TOC header readability; Update History v1.0.1 entry",
        [
            "PDF report table of contents: column headers now use white background "
            "with navy bold text and a navy underline rule for clear readability",
            "Added missing v1.0.1 entry to Update History",
        ],
    ),
    (
        "1.0.10",
        "Installer version fix; receipt image in PDF report",
        [
            "Fixed the version shown in Windows Settings > Apps > Installed Apps "
            "— the installer was still reporting v1.0.2",
            "PDF inventory report now includes the receipt image for each asset "
            "that has one (image receipts embedded; PDF receipts noted by filename)",
        ],
    ),
    (
        "1.0.9",
        "Bug fix — PDF report generation",
        [
            "Fixed a crash when generating the PDF report caused by an invalid "
            "ReportLab Table parameter (colPaddings)",
        ],
    ),
    (
        "1.0.8",
        "Legal-grade PDF inventory report",
        [
            "Complete redesign of the PDF report for insurance claim submission",
            "Formal cover page with declaration statement and owner signature line",
            "Running header (document title, date) and footer (confidential notice, "
            "page number) on every page",
            "Assets grouped by category with a navy category divider page",
            "Structured per-asset records with two-column detail grid, "
            "notes section, and 3-column photo grid with file name captions",
            "Professional navy/slate colour scheme replacing the previous "
            "casual blue palette",
        ],
    ),
    (
        "1.0.7",
        "Receipt link and UI improvements",
        [
            "Receipt field in the asset details panel is now a clickable link "
            "that opens the receipt file in the default viewer",
            "Receipt checkmark (✓) is now displayed in green",
        ],
    ),
    (
        "1.0.6",
        "New categories and field rename",
        [
            'Added "Firearms" and "Personal Effects" asset categories',
            'Renamed "Purchase Value" to "Purchase Price" throughout the app '
            "(forms, list, preview panel, PDF report, CSV export, status bar)",
        ],
    ),
    (
        "1.0.5",
        "Installer and Help improvements",
        [
            "Removed version number from installer dialog titles",
            "Added Update History dialog (Help menu)",
        ],
    ),
    (
        "1.0.4",
        "Python installer progress bar",
        [
            "Python installation now shows a progress bar dialog instead "
            "of running invisibly in the background",
        ],
    ),
    (
        "1.0.3",
        "About page data paths",
        [
            "About dialog now displays the full path to the database file "
            "and the assets folder",
            "Paths are selectable by mouse for easy copying",
        ],
    ),
    (
        "1.0.2",
        "Layout improvements",
        [
            "Asset table and preview panel each take 50% of the window width "
            "and scale together when the window is resized",
            "Divider line below the toolbar always spans the full window width",
            "Toolbar, table, and preview panel anchored to the top — "
            "enlarging the window vertically adds empty space below",
        ],
    ),
    (
        "1.0.1",
        "Layout — sticky content anchored to top",
        [
            "Toolbar, asset table, and preview panel anchored to the top of "
            "the window; enlarging the window vertically adds empty space below "
            "rather than stretching the content",
        ],
    ),
    (
        "1.0.0",
        "Initial release",
        [
            "Track household assets with photos, sales receipts, purchase date, "
            "purchase price, and current/appraised value",
            "Assign assets to categories (Living Room, Kitchen, Garage, etc.) "
            "with serial number and model number fields",
            "Search assets by name and filter by category",
            "Preview panel shows thumbnails — click any photo to view full size",
            "Right-click menu and bulk multi-select delete",
            "Duplicate an existing asset to quickly add similar items",
            "Encrypted ZIP backup (AES-256) with password, and restore from backup",
            "PDF report with title page, table of contents, and per-asset pages "
            "grouped by category — suitable for insurance claims",
            "CSV export compatible with Excel",
            "Auto-backup prompt on exit (optional, toggleable in File menu)",
            "Live status bar showing asset count and total purchase/current values",
            "Windows installer with automatic Python detection and installation",
        ],
    ),
]


class UpdateHistoryDialog(QDialog):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle(f"{APP_NAME} — Update History")
        self.setMinimumWidth(520)
        self.setMinimumHeight(480)
        self.resize(560, 560)

        outer = QVBoxLayout(self)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        outer.addWidget(scroll)

        container = QWidget()
        scroll.setWidget(container)
        layout = QVBoxLayout(container)
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        layout.setSpacing(16)

        for version, title, bullets in HISTORY:
            # Version heading
            heading = QLabel(f"<b>v{version}</b> &nbsp; {title}")
            heading.setStyleSheet("font-size: 13px; color: #1a5276;")
            layout.addWidget(heading)

            # Bullet points
            for bullet in bullets:
                row = QLabel(f"  •  {bullet}")
                row.setWordWrap(True)
                row.setStyleSheet("color: #333; font-size: 11px;")
                layout.addWidget(row)

            # Divider between versions
            line = QFrame()
            line.setFrameShape(QFrame.Shape.HLine)
            line.setStyleSheet("color: #e0e0e0;")
            layout.addWidget(line)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Close)
        buttons.rejected.connect(self.accept)
        outer.addWidget(buttons)
