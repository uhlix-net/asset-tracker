; ============================================================
;  Asset Tracker — NSIS Installer Script
;  Requires NSIS 3.x  (https://nsis.sourceforge.io)
; ============================================================

Unicode True

!include "MUI2.nsh"
!include "LogicLib.nsh"

; ── Definitions ─────────────────────────────────────────────
!define APP_NAME        "Asset Tracker"
!define APP_VERSION     "1.0.0"
!define APP_PUBLISHER   "uhlix-net"
!define APP_EXE         "launch.vbs"
!define UNINST_KEY      "Software\Microsoft\Windows\CurrentVersion\Uninstall\AssetTracker"
!define PYTHON_URL      "https://www.python.org/ftp/python/3.12.9/python-3.12.9-amd64.exe"
!define PYTHON_MIN      "3.10"

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
!define MUI_WELCOMEPAGE_TITLE   "Welcome to ${APP_NAME} Setup"
!define MUI_WELCOMEPAGE_TEXT    \
    "${APP_NAME} helps you document household assets for insurance purposes.\
    $\r$\n$\r$\nClick Next to continue."
!define MUI_FINISHPAGE_RUN          "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT     "Launch ${APP_NAME} now"
!define MUI_FINISHPAGE_RUN_FUNCTION LaunchApp

; ── Pages ───────────────────────────────────────────────────
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Variables ───────────────────────────────────────────────
Var PythonExe   ; Full path to python.exe once found/installed

; ============================================================
;  Helper: Find Python 3.10+ in registry or PATH
;  Sets $PythonExe on success, leaves it empty on failure.
; ============================================================
Function FindPython
    StrCpy $PythonExe ""

    ; Check HKCU then HKLM for each supported version
    !macro _TryReg ROOT VER
        ReadRegStr $R0 ${ROOT} "SOFTWARE\Python\PythonCore\${VER}\InstallPath" ""
        ${If} $R0 != ""
        ${AndIf} $PythonExe == ""
            IfFileExists "$R0\python.exe" 0 +2
                StrCpy $PythonExe "$R0\python.exe"
        ${EndIf}
    !macroend

    !insertmacro _TryReg HKCU "3.13"
    !insertmacro _TryReg HKCU "3.12"
    !insertmacro _TryReg HKCU "3.11"
    !insertmacro _TryReg HKCU "3.10"
    !insertmacro _TryReg HKLM "3.13"
    !insertmacro _TryReg HKLM "3.12"
    !insertmacro _TryReg HKLM "3.11"
    !insertmacro _TryReg HKLM "3.10"

    ; Fall back to PATH lookup
    ${If} $PythonExe == ""
        nsExec::ExecToStack 'python --version'
        Pop $R0  ; exit code
        Pop $R1  ; output  e.g. "Python 3.12.4"
        ${If} $R0 == 0
            ; Verify version >= 3.10
            ${If} $R1 =~ "Python 3\.(1[0-9]|[2-9][0-9])"
                StrCpy $PythonExe "python"
            ${EndIf}
        ${EndIf}
    ${EndIf}
FunctionEnd

; ============================================================
;  Helper: Download Python via PowerShell, then install silently
; ============================================================
Function DownloadAndInstallPython
    DetailPrint "Downloading Python ${PYTHON_MIN}+ from python.org..."

    ; Write a small PowerShell download script to avoid quoting issues
    FileOpen  $R9 "$TEMP\dl_python.ps1" w
    FileWrite $R9 "try {$\n"
    FileWrite $R9 "  (New-Object System.Net.WebClient).DownloadFile($\n"
    FileWrite $R9 "    '${PYTHON_URL}',$\n"
    FileWrite $R9 "    (Join-Path $env:TEMP 'python_setup.exe')$\n"
    FileWrite $R9 "  )$\n"
    FileWrite $R9 "  exit 0$\n"
    FileWrite $R9 "} catch { exit 1 }$\n"
    FileClose $R9

    ExecWait 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$TEMP\dl_python.ps1"' $R0
    Delete "$TEMP\dl_python.ps1"

    ${If} $R0 != 0
        MessageBox MB_OK|MB_ICONSTOP \
            "Could not download Python. Please install Python ${PYTHON_MIN}+ \
            manually from https://www.python.org and re-run this installer."
        Abort
    ${EndIf}

    DetailPrint "Installing Python (this may take a moment)..."
    ExecWait '"$TEMP\python_setup.exe" /quiet InstallAllUsers=1 PrependPath=1 Include_pip=1 Include_tcltk=1' $R1
    Delete "$TEMP\python_setup.exe"

    ${If} $R1 != 0
        MessageBox MB_OK|MB_ICONSTOP \
            "Python installation failed (code $R1). Please install Python \
            ${PYTHON_MIN}+ from https://www.python.org and re-run this installer."
        Abort
    ${EndIf}

    ; Re-read registry for the newly installed Python
    Call FindPython
    ${If} $PythonExe == ""
        ; PATH not yet updated in this process — read registry directly
        ReadRegStr $R0 HKLM "SOFTWARE\Python\PythonCore\3.12\InstallPath" ""
        ${If} $R0 != ""
            StrCpy $PythonExe "$R0\python.exe"
        ${EndIf}
    ${EndIf}
FunctionEnd

; ============================================================
;  Check Python on installer start and offer to install
; ============================================================
Function .onInit
    Call FindPython

    ${If} $PythonExe == ""
        MessageBox MB_YESNO|MB_ICONQUESTION \
            "Python ${PYTHON_MIN} or newer is required but was not found on this system.\
            $\r$\n$\r$\nWould you like to download and install Python 3.12 now?\
            $\r$\n$\r$\nClick Yes to download Python automatically, or No to cancel.\
            $\r$\n(~27 MB download from python.org)" \
            IDYES +2
            Abort   ; User said No — cancel installer

        Call DownloadAndInstallPython

        ${If} $PythonExe == ""
            MessageBox MB_OK|MB_ICONSTOP \
                "Python was installed but could not be located.$\r$\n\
                Please restart this installer."
            Abort
        ${EndIf}

        MessageBox MB_OK|MB_ICONINFORMATION "Python installed successfully. Setup will now continue."
    ${EndIf}
FunctionEnd

; ============================================================
;  Main install section
; ============================================================
Section "${APP_NAME}" SecMain
    SectionIn RO   ; required — cannot be deselected

    ; ── Copy application source files ──────────────────────
    SetOutPath "$INSTDIR"
    File "..\main.py"
    File "..\requirements.txt"

    CreateDirectory "$INSTDIR\asset_tracker"
    SetOutPath "$INSTDIR\asset_tracker"
    File "..\asset_tracker\*.py"

    CreateDirectory "$INSTDIR\asset_tracker\ui"
    SetOutPath "$INSTDIR\asset_tracker\ui"
    File "..\asset_tracker\ui\*.py"

    ; ── VBScript launcher (no console window) ──────────────
    SetOutPath "$INSTDIR"
    FileOpen  $0 "$INSTDIR\launch.vbs" w
    FileWrite $0 "Set ws = CreateObject(""WScript.Shell"")" & "$\r$\n"
    FileWrite $0 "ws.CurrentDirectory = ""$INSTDIR""" & "$\r$\n"
    FileWrite $0 "ws.Run Chr(34) & ""$INSTDIR\.venv\Scripts\pythonw.exe"" & Chr(34) & "" "" & Chr(34) & ""$INSTDIR\main.py"" & Chr(34), 0" & "$\r$\n"
    FileClose $0

    ; ── Create virtual environment ─────────────────────────
    DetailPrint "Creating Python virtual environment..."
    nsExec::ExecToLog '"$PythonExe" -m venv "$INSTDIR\.venv"'
    Pop $R0
    ${If} $R0 != 0
        MessageBox MB_OK|MB_ICONSTOP "Failed to create virtual environment (code $R0)."
        Abort
    ${EndIf}

    ; ── Install dependencies ───────────────────────────────
    DetailPrint "Installing dependencies — this may take a minute..."
    nsExec::ExecToLog '"$INSTDIR\.venv\Scripts\pip.exe" install --quiet -r "$INSTDIR\requirements.txt"'
    Pop $R0
    ${If} $R0 != 0
        MessageBox MB_YESNO|MB_ICONEXCLAMATION \
            "Dependency installation returned code $R0. The app may not work correctly.$\r$\n\
            $\r$\nContinue anyway?" IDYES +2
            Abort
    ${EndIf}

    ; ── Shortcuts ──────────────────────────────────────────
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"

    ; Start Menu — app
    CreateShortcut \
        "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" \
        "wscript.exe" \
        '"$INSTDIR\launch.vbs"' \
        "" 0 SW_SHOWMINIMIZED "" "${APP_NAME}"

    ; Start Menu — uninstall
    CreateShortcut \
        "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk" \
        "$INSTDIR\Uninstall.exe"

    ; Desktop
    CreateShortcut \
        "$DESKTOP\${APP_NAME}.lnk" \
        "wscript.exe" \
        '"$INSTDIR\launch.vbs"' \
        "" 0 SW_SHOWMINIMIZED "" "${APP_NAME}"

    ; ── Registry (Add/Remove Programs) ─────────────────────
    WriteRegStr   HKLM "${UNINST_KEY}" "DisplayName"    "${APP_NAME}"
    WriteRegStr   HKLM "${UNINST_KEY}" "DisplayVersion"  "${APP_VERSION}"
    WriteRegStr   HKLM "${UNINST_KEY}" "Publisher"       "${APP_PUBLISHER}"
    WriteRegStr   HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr   HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify"        1
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair"        1

    ; Estimate installed size in KB
    SectionGetSize ${SecMain} $R0
    WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" $R0

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
    ; Shortcuts
    Delete "$DESKTOP\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk"
    RMDir  "$SMPROGRAMS\${APP_NAME}"

    ; Virtual environment and installed files
    RMDir /r "$INSTDIR\.venv"
    Delete "$INSTDIR\main.py"
    Delete "$INSTDIR\requirements.txt"
    Delete "$INSTDIR\launch.vbs"
    RMDir /r "$INSTDIR\asset_tracker"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir  "$INSTDIR"   ; removes dir only if empty (preserves user data)

    ; Registry
    DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
