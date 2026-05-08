; ============================================================
;  Asset Tracker — NSIS Installer Script
;  Requires NSIS 3.x  (https://nsis.sourceforge.io)
; ============================================================

Unicode True

!include "MUI2.nsh"
!include "LogicLib.nsh"

; ── Definitions ─────────────────────────────────────────────
!define APP_NAME      "Asset Tracker"
!define APP_VERSION   "1.0.2"
!define APP_PUBLISHER "uhlix-net"
!define UNINST_KEY    "Software\Microsoft\Windows\CurrentVersion\Uninstall\AssetTracker"
!define PYTHON_URL    "https://www.python.org/ftp/python/3.12.9/python-3.12.9-amd64.exe"
!define PYTHON_MIN    "3.10"

; ── General ─────────────────────────────────────────────────
Name                  "${APP_NAME} ${APP_VERSION}"
OutFile               "AssetTracker_Setup.exe"
InstallDir            "$PROGRAMFILES64\AssetTracker"
InstallDirRegKey HKLM "${UNINST_KEY}" "InstallLocation"
RequestExecutionLevel admin
ShowInstDetails       show
SetCompressor         lzma

; ── MUI Settings ────────────────────────────────────────────
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to ${APP_NAME} Setup"
!define MUI_WELCOMEPAGE_TEXT \
    "${APP_NAME} helps you document household assets for insurance purposes.\
    $\r$\n$\r$\nClick Next to continue."
!define MUI_FINISHPAGE_RUN_TEXT     "Launch ${APP_NAME} now"
!define MUI_FINISHPAGE_RUN_FUNCTION LaunchApp
!define MUI_FINISHPAGE_RUN          "wscript.exe"

; ── Pages ───────────────────────────────────────────────────
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Variable ────────────────────────────────────────────────
Var PythonExe

; ============================================================
;  Macro: probe one Python registry key for python.exe
;
;  NSIS is a 32-bit process.  Python 64-bit registers in the
;  64-bit registry hive, which 32-bit processes cannot see
;  without SetRegView 64.  Always call this macro inside a
;  SetRegView 64 / SetRegView 32 block.
;
;  "ExecutablePath" stores the full path to python.exe directly,
;  avoiding the trailing-backslash issue with the "InstallPath"
;  default value (e.g. C:\Python312\ -> double-backslash path).
; ============================================================
!macro TryRegKey ROOT VER
    ${If} $PythonExe == ""
        ReadRegStr $R0 ${ROOT} "SOFTWARE\Python\PythonCore\${VER}\InstallPath" "ExecutablePath"
        ${If} $R0 != ""
            IfFileExists "$R0" 0 +2
                StrCpy $PythonExe "$R0"
        ${EndIf}
    ${EndIf}
!macroend

; ============================================================
;  FindPython — checks 64-bit registry (3.10-3.13) then PATH
; ============================================================
Function FindPython
    StrCpy $PythonExe ""

    ; Read the 64-bit registry hive so Python 64-bit is visible
    SetRegView 64
    !insertmacro TryRegKey HKCU "3.13"
    !insertmacro TryRegKey HKCU "3.12"
    !insertmacro TryRegKey HKCU "3.11"
    !insertmacro TryRegKey HKCU "3.10"
    !insertmacro TryRegKey HKLM "3.13"
    !insertmacro TryRegKey HKLM "3.12"
    !insertmacro TryRegKey HKLM "3.11"
    !insertmacro TryRegKey HKLM "3.10"
    SetRegView 32

    ; Fall back to PATH — accept any Python 3.x in the current environment
    ${If} $PythonExe == ""
        nsExec::ExecToStack 'python --version'
        Pop $R0   ; exit code
        Pop $R1   ; output, e.g. "Python 3.12.4"
        ${If} $R0 == 0
            StrCpy $R2 $R1 8       ; first 8 chars: "Python 3"
            ${If} $R2 == "Python 3"
                StrCpy $PythonExe "python"
            ${EndIf}
        ${EndIf}
    ${EndIf}
FunctionEnd

; ============================================================
;  DownloadAndInstallPython
; ============================================================
Function DownloadAndInstallPython
    DetailPrint "Downloading Python 3.12 from python.org..."

    ; Write a PowerShell download script — avoids quoting issues
    FileOpen  $R9 "$TEMP\dl_python.ps1" w
    FileWrite $R9 "try {$\n"
    FileWrite $R9 "  (New-Object System.Net.WebClient).DownloadFile($\n"
    FileWrite $R9 "    '${PYTHON_URL}',$\n"
    FileWrite $R9 "    (Join-Path ([System.IO.Path]::GetTempPath()) 'python_setup.exe')$\n"
    FileWrite $R9 "  )$\n"
    FileWrite $R9 "  exit 0$\n"
    FileWrite $R9 "} catch { exit 1 }$\n"
    FileClose $R9

    nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$TEMP\dl_python.ps1"'
    Pop $R0
    Delete "$TEMP\dl_python.ps1"

    ${If} $R0 != 0
        MessageBox MB_OK|MB_ICONSTOP \
            "Could not download Python.$\r$\nPlease install Python ${PYTHON_MIN}+ \
            from https://www.python.org and re-run this installer."
        Abort
    ${EndIf}

    DetailPrint "Installing Python 3.12..."
    ; /passive shows a progress bar dialog and completes without user interaction
    ExecWait '"$TEMP\python_setup.exe" /passive InstallAllUsers=1 PrependPath=1 Include_pip=1 Include_tcltk=1' $R1
    Delete "$TEMP\python_setup.exe"

    ${If} $R1 != 0
        MessageBox MB_OK|MB_ICONSTOP \
            "Python installation failed (code $R1).$\r$\n\
            Please install Python ${PYTHON_MIN}+ from https://www.python.org and re-run."
        Abort
    ${EndIf}

    ; Read the 64-bit hive directly — PATH is not refreshed in the
    ; current process after a silent install, but the registry is.
    SetRegView 64
    ReadRegStr $R0 HKLM "SOFTWARE\Python\PythonCore\3.12\InstallPath" "ExecutablePath"
    SetRegView 32
    ${If} $R0 != ""
        IfFileExists "$R0" 0 +2
            StrCpy $PythonExe "$R0"
    ${EndIf}

    ; Last resort: re-run the full registry scan
    ${If} $PythonExe == ""
        Call FindPython
    ${EndIf}
FunctionEnd

; ============================================================
;  .onInit — Python check before any pages appear
; ============================================================
Function .onInit
    Call FindPython
    ${If} $PythonExe == ""
        MessageBox MB_YESNO|MB_ICONQUESTION \
            "Python ${PYTHON_MIN} or newer is required but was not found.$\r$\n$\r$\n\
            Would you like to download and install Python 3.12 now?$\r$\n\
            (~27 MB download from python.org)$\r$\n$\r$\n\
            Click Yes to install Python automatically, or No to cancel." \
            IDYES +2
            Abort

        Call DownloadAndInstallPython

        ${If} $PythonExe == ""
            MessageBox MB_OK|MB_ICONSTOP \
                "Python was installed but could not be located.$\r$\n\
                Please restart this installer."
            Abort
        ${EndIf}

        MessageBox MB_OK|MB_ICONINFORMATION \
            "Python installed successfully. Setup will now continue."
    ${EndIf}
FunctionEnd

; ============================================================
;  Main install section
; ============================================================
Section "${APP_NAME}" SecMain
    SectionIn RO

    ; ── Copy application files ─────────────────────────────
    SetOutPath "$INSTDIR"
    File "..\main.py"
    File "..\requirements.txt"

    CreateDirectory "$INSTDIR\asset_tracker"
    SetOutPath "$INSTDIR\asset_tracker"
    File "..\asset_tracker\*.py"

    CreateDirectory "$INSTDIR\asset_tracker\ui"
    SetOutPath "$INSTDIR\asset_tracker\ui"
    File "..\asset_tracker\ui\*.py"

    ; ── VBScript launcher (suppresses console window) ──────
    SetOutPath "$INSTDIR"
    FileOpen $0 "$INSTDIR\launch.vbs" w
    FileWrite $0 "Set ws = CreateObject($\"WScript.Shell$\")$\r$\n"
    FileWrite $0 "ws.CurrentDirectory = $\"$INSTDIR$\"$\r$\n"
    FileWrite $0 "ws.Run Chr(34) & $\"$INSTDIR\.venv\Scripts\pythonw.exe$\" & Chr(34) & $\" $\" & Chr(34) & $\"$INSTDIR\main.py$\" & Chr(34), 0$\r$\n"
    FileClose $0

    ; ── Create virtual environment ─────────────────────────
    DetailPrint "Creating Python virtual environment..."
    nsExec::ExecToLog '"$PythonExe" -m venv "$INSTDIR\.venv"'
    Pop $R0
    ${If} $R0 != 0
        MessageBox MB_OK|MB_ICONSTOP \
            "Failed to create virtual environment (code $R0)."
        Abort
    ${EndIf}

    ; ── Install dependencies ───────────────────────────────
    DetailPrint "Installing dependencies (this may take a minute)..."
    nsExec::ExecToLog '"$INSTDIR\.venv\Scripts\pip.exe" install --quiet -r "$INSTDIR\requirements.txt"'
    Pop $R0
    ${If} $R0 != 0
        MessageBox MB_YESNO|MB_ICONEXCLAMATION \
            "Dependency installation returned code $R0.$\r$\n\
            The app may not work correctly.$\r$\nContinue anyway?" \
            IDYES +2
            Abort
    ${EndIf}

    ; ── Start Menu and Desktop shortcuts ───────────────────
    StrCpy $R7 "$WINDIR\System32\wscript.exe"

    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" \
        "$R7" '"$INSTDIR\launch.vbs"' \
        "" 0 SW_SHOWNORMAL "" "${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk" \
        "$INSTDIR\Uninstall.exe"
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" \
        "$R7" '"$INSTDIR\launch.vbs"' \
        "" 0 SW_SHOWNORMAL "" "${APP_NAME}"

    ; ── Add/Remove Programs registry entries ───────────────
    WriteRegStr   HKLM "${UNINST_KEY}" "DisplayName"     "${APP_NAME}"
    WriteRegStr   HKLM "${UNINST_KEY}" "DisplayVersion"  "${APP_VERSION}"
    WriteRegStr   HKLM "${UNINST_KEY}" "Publisher"       "${APP_PUBLISHER}"
    WriteRegStr   HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr   HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify"        1
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair"        1
    SectionGetSize ${SecMain} $R0
    WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize"   $R0

    WriteUninstaller "$INSTDIR\Uninstall.exe"
    DetailPrint "Installation complete."
SectionEnd

; ============================================================
;  Finish page: launch the app
; ============================================================
Function LaunchApp
    Exec 'wscript.exe "$INSTDIR\launch.vbs"'
FunctionEnd

; ============================================================
;  Uninstaller
; ============================================================
Section "Uninstall"
    Delete "$DESKTOP\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk"
    RMDir  "$SMPROGRAMS\${APP_NAME}"

    RMDir /r "$INSTDIR\.venv"
    Delete "$INSTDIR\main.py"
    Delete "$INSTDIR\requirements.txt"
    Delete "$INSTDIR\launch.vbs"
    RMDir /r "$INSTDIR\asset_tracker"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir  "$INSTDIR"

    DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
