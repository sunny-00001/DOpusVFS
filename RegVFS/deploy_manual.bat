@echo off
echo Deploying RegistryVFS.dll to DOpus...
echo.

echo Step 1: Terminating Directory Opus processes...
taskkill /F /IM dopus.exe 2>nul
taskkill /F /IM dopusrt.exe 2>nul
timeout /t 2 /nobreak >nul

echo Step 2: Copying RegistryVFS.dll...
copy /Y "d:\VFS\RegistryVFS.dll" "D:\Dopus\VFSPlugins\RegistryVFS.dll"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Deployment successful!
    echo.
    echo Step 3: Starting Directory Opus...
    start "" "D:\Dopus\dopus.exe"
    echo Done.
) else (
    echo.
    echo Deployment failed. Please check if DOpus is still running.
)

pause