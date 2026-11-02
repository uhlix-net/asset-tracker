!include "..\build\version.nsh"   ; VERSION, written by CMake from project()

!define APPNAME "AssetTracker"
!define DISPLAYNAME "Asset Tracker"
!define EXENAME "asset_tracker.exe"
!define PUBLISHER "uhlix-net"
; The Python edition's key on purpose: this installer upgrades it in place.
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\AssetTracker"

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

; "1" when Asset Tracker was running as this installer started, so it should be
; brought back up afterwards. Set in .onInit, read in .onInstSuccess.
Var WasRunning

!define MUI_ICON "app.ico"
!define MUI_UNICON "app.ico"

Name "${DISPLAYNAME}"
OutFile "Output\AssetTracker_Setup.exe"
InstallDir "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey HKLM "${UNINSTKEY}" "InstallLocation"
RequestExecutionLevel admin

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

; Pushes "1" when the C++ app is running, "0" otherwise.
; find returns 0 when it matches a line, 1 when it does not.
Function IsAppRunning
  nsExec::ExecToStack 'cmd.exe /c tasklist /FI "IMAGENAME eq ${EXENAME}" /NH | find /I "${EXENAME}"'
  Pop $R0   ; exit code
  Pop $R1   ; captured output (unused)
  StrCmp $R0 "0" running notrunning
  running:
    Push "1"
    Return
  notrunning:
    Push "0"
FunctionEnd

; The Python edition runs as pythonw.exe from this install folder, or with
; Asset Tracker in the title. Any other pythonw process is left alone.
Function StopPythonEdition
  FileOpen  $R9 "$TEMP\close_at.ps1" w
  FileWrite $R9 '$$p = Get-Process -Name pythonw -EA SilentlyContinue$\r$\n'
  FileWrite $R9 '$$p | Where-Object { ($$_.Path -like "*AssetTracker*") -or ($$_.MainWindowTitle -like "*AssetTracker*") } | ForEach-Object { $$_.CloseMainWindow() | Out-Null }$\r$\n'
  FileWrite $R9 'Start-Sleep -Seconds 2$\r$\n'
  FileWrite $R9 '$$p | Where-Object { -not $$_.HasExited -and (($$_.Path -like "*AssetTracker*") -or ($$_.MainWindowTitle -like "*AssetTracker*")) } | Stop-Process -Force -EA SilentlyContinue$\r$\n'
  FileClose $R9
  nsExec::Exec 'powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "$TEMP\close_at.ps1"'
  Pop $0
  Delete "$TEMP\close_at.ps1"
FunctionEnd

Function .onInit
  SetRegView 64

  StrCpy $WasRunning "0"

  ; Bring the installer window to the front so it isn't left behind
  ; other open windows.
  BringToFront

  Call IsAppRunning
  Pop $0
  StrCmp $0 "0" not_running

  ; Running now, so put it back afterwards.
  StrCpy $WasRunning "1"
  MessageBox MB_YESNO|MB_ICONQUESTION \
    "Asset Tracker is currently running.$\n$\nThe installer must close it before continuing.$\n$\nClose Asset Tracker now?" \
    /SD IDYES IDYES close_app IDNO abort_install

  close_app:
    ; taskkill without /F posts WM_CLOSE, so the app shuts down cleanly.
    nsExec::Exec 'taskkill.exe /IM ${EXENAME}'
    Pop $0
    Sleep 2000
    Call IsAppRunning
    Pop $0
    StrCmp $0 "0" not_running
    nsExec::Exec 'taskkill.exe /F /IM ${EXENAME}'
    Pop $0
    Sleep 500
    Goto not_running

  abort_install:
    Abort

  not_running:

  ; If already installed, ask whether to install/update or cancel
  ReadRegStr $R0 HKLM "${UNINSTKEY}" "DisplayVersion"
  StrCmp $R0 "" not_installed
  MessageBox MB_YESNO|MB_ICONQUESTION "${DISPLAYNAME} version $R0 is already installed.$\n$\nInstall version ${VERSION} now?" /SD IDYES IDYES not_installed
  Abort
  not_installed:
FunctionEnd

; Relaunch only if Asset Tracker was open when this installer started. Launching
; through explorer.exe runs it at the user's own integrity level instead of
; inheriting the installer's admin token.
Function .onInstSuccess
  ${If} $WasRunning == "1"
    Exec '"$WINDIR\explorer.exe" "$INSTDIR\${EXENAME}"'
  ${EndIf}
FunctionEnd

Section "Application" SecApp
  SetRegView 64

  ; Upgrade from the Python edition. Only program files go: user data lives in
  ; %APPDATA%\AssetTracker and is not touched.
  Call StopPythonEdition
  RMDir /r "$INSTDIR\.venv"
  RMDir /r "$INSTDIR\asset_tracker"
  Delete "$INSTDIR\main.py"
  Delete "$INSTDIR\requirements.txt"
  Delete "$INSTDIR\launch.vbs"
  Delete "$INSTDIR\resources\app_icon.ico"
  RMDir  "$INSTDIR\resources"

; --- Application binary + all runtime files (Qt, plugins, vcpkg DLLs) ---
  ; Packages everything windeployqt + vcpkg staged in build\Release.
  SetOutPath "$INSTDIR"
  File /r /x *.pdb /x *.lib /x *.exp /x asset_tracker_tests.exe "..\build\Release\*.*"
  File "app.ico"

  ; Uninstaller and registration come first so that nothing after them can skip
  ; them. An NSIS section that dies part-way is silent.
  ClearErrors
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  ${If} ${Errors}
    DetailPrint "ERROR: could not write $INSTDIR\Uninstall.exe"
    ${IfNot} ${Silent}
      MessageBox MB_OK|MB_ICONEXCLAMATION \
        "Could not write the uninstaller to:$\n$INSTDIR\Uninstall.exe$\n$\nThe application is installed, but will not appear in Apps & Features."
    ${EndIf}
  ${EndIf}

  ; Register with Windows "Apps & Features"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName"      "${DISPLAYNAME}"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString"  '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation"  "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher"        "${PUBLISHER}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion"   "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon"      "$INSTDIR\app.ico"
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1

  ; Size in KB, for Apps & Features
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${UNINSTKEY}" "EstimatedSize" "$0"

  ; Delete existing shortcuts before recreating so the .lnk icon cache is flushed.
  ; These replace the Python edition's, which pointed at launch.vbs.
  Delete "$SMPROGRAMS\${DISPLAYNAME}\${DISPLAYNAME}.lnk"
  Delete "$SMPROGRAMS\${DISPLAYNAME}\Uninstall ${DISPLAYNAME}.lnk"
  Delete "$DESKTOP\${DISPLAYNAME}.lnk"

  ; Shortcuts — use exe as icon source so the embedded resource is read directly
  CreateDirectory "$SMPROGRAMS\${DISPLAYNAME}"
  CreateShortcut "$SMPROGRAMS\${DISPLAYNAME}\${DISPLAYNAME}.lnk" \
    "$INSTDIR\${EXENAME}" "" "$INSTDIR\${EXENAME}" 0
  CreateShortcut "$SMPROGRAMS\${DISPLAYNAME}\Uninstall ${DISPLAYNAME}.lnk" \
    "$INSTDIR\Uninstall.exe" "" "" 0
  CreateShortcut "$DESKTOP\${DISPLAYNAME}.lnk" \
    "$INSTDIR\${EXENAME}" "" "$INSTDIR\${EXENAME}" 0

  ; Shell icon cache refresh (SHCNE_ASSOCCHANGED | SHCNF_FLUSHNOWAIT). "i", not
  ; "l": the arguments are 4 bytes each, and FLUSHNOWAIT because a blocking flush
  ; took Star Term's installer down mid-section for ten weeks.
  System::Call "Shell32::SHChangeNotify(i 0x8000000, i 0x2000, p 0, p 0)"
SectionEnd

Section "Uninstall"
  SetRegView 64
  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\${DISPLAYNAME}\${DISPLAYNAME}.lnk"
  Delete "$SMPROGRAMS\${DISPLAYNAME}\Uninstall ${DISPLAYNAME}.lnk"
  RMDir  "$SMPROGRAMS\${DISPLAYNAME}"
  Delete "$DESKTOP\${DISPLAYNAME}.lnk"
  DeleteRegKey HKLM "${UNINSTKEY}"
SectionEnd
