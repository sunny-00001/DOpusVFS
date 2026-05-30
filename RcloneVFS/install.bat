@echo off
setlocal

echo ========================================
echo RcloneVFS Plugin Installer
echo ========================================
echo.

set SRCDIR=%~dp0
set ROOTDIR=%SRCDIR%..
set DLLFILE=%ROOTDIR%\RcloneVFS.dll

if not exist "%DLLFILE%" (
    echo ERROR: RcloneVFS.dll not found!
    echo Expected location: %DLLFILE%
    echo Please run build.bat first.
    pause
    exit /b 1
)

set DOPUSDIR=%ProgramFiles%\GPSoftware\Directory Opus
set VFSPLUGINDIR=%DOPUSDIR%\VFSPlugins

if not exist "%DOPUSDIR%" (
    echo ERROR: Directory Opus not found at:
    echo %DOPUSDIR%
    echo.
    echo Please specify the correct Directory Opus installation path.
    pause
    exit /b 1
)

if not exist "%VFSPLUGINDIR%" mkdir "%VFSPLUGINDIR%"

echo Installing RcloneVFS.dll to:
echo %VFSPLUGINDIR%
echo.

copy /Y "%DLLFILE%" "%VFSPLUGINDIR%\"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Installation successful!
    echo ========================================
    echo.
    echo Next steps:
    echo 1. Restart Directory Opus
    echo 2. Type rclone:// in the address bar
    echo 3. Browse your cloud storage!
    echo.
) else (
    echo.
    echo ERROR: Installation failed!
    echo You may need to run this script as Administrator.
)

pause
