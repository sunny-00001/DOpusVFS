@echo off
setlocal enabledelayedexpansion

echo ========================================
echo WIFIVFS Plugin Install Script
echo ========================================
echo.

set ROOTDIR=%~dp0..
set DLLFILE=%ROOTDIR%\WIFIVFS.dll
set DOPUSDIR=%ProgramFiles%\GPSoftware\Directory Opus
set VFSPLUGINDIR=%DOPUSDIR%\VFSPlugins

if not exist "%DLLFILE%" (
    echo Error: WIFIVFS.dll not found at %DLLFILE%
    echo Please build the plugin first.
    pause
    exit /b 1
)

if not exist "%VFSPLUGINDIR%" (
    echo Creating VFSPlugins directory...
    mkdir "%VFSPLUGINDIR%"
)

echo Installing WIFIVFS.dll to %VFSPLUGINDIR%...
copy /Y "%DLLFILE%" "%VFSPLUGINDIR%\"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Installation successful!
    echo ========================================
    echo.
    echo Please restart Directory Opus to load the plugin.
    echo Then type wifi:// in the address bar.
) else (
    echo.
    echo Installation failed! Try running as Administrator.
)

pause
endlocal
