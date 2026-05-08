@echo off
setlocal

:: ── Locate makensis ──────────────────────────────────────────────────────────
set MAKENSIS=
if exist "%PROGRAMFILES(X86)%\NSIS\makensis.exe" set MAKENSIS=%PROGRAMFILES(X86)%\NSIS\makensis.exe
if exist "%PROGRAMFILES%\NSIS\makensis.exe"       set MAKENSIS=%PROGRAMFILES%\NSIS\makensis.exe

if "%MAKENSIS%"=="" (
    echo ERROR: NSIS not found.
    echo.
    echo Please install NSIS 3.x from https://nsis.sourceforge.io/Download
    echo then re-run this script.
    pause
    exit /b 1
)

:: ── Build ────────────────────────────────────────────────────────────────────
echo Building Asset Tracker installer...
echo Using: %MAKENSIS%
echo.

cd /d "%~dp0"
"%MAKENSIS%" installer.nsi

if %ERRORLEVEL% neq 0 (
    echo.
    echo BUILD FAILED.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ============================================================
echo  SUCCESS: AssetTracker_Setup.exe created in installer\
echo ============================================================
pause
