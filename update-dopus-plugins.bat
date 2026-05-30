@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Directory Opus Plugin Updater
echo ========================================
echo.

set SRCDIR=d:\VFS
set DOPUSDIR=D:\Dopus
set PLUGINDIR=%DOPUSDIR%\VFSPlugins

echo Checking source directory: %SRCDIR%
if not exist "%SRCDIR%" (
    echo ERROR: Source directory %SRCDIR% does not exist!
    pause
    exit /b 1
)

echo Checking DOpus directory: %DOPUSDIR%
if not exist "%DOPUSDIR%" (
    echo ERROR: DOpus directory %DOPUSDIR% does not exist!
    pause
    exit /b 1
)

if not exist "%PLUGINDIR%" (
    echo Creating plugin directory: %PLUGINDIR%
    mkdir "%PLUGINDIR%"
)

echo.
echo Step 1: Terminating Directory Opus processes...
taskkill /f /im dopus.exe 2>nul
taskkill /f /im dopusrt.exe 2>nul
echo Waiting for processes to fully exit...
timeout /t 3 /nobreak >nul
echo Done.

echo.
echo Step 2: Updating plugins from %SRCDIR% to %PLUGINDIR%...
set UPDATED=0
set FAILED=0

for %%f in ("%SRCDIR%\*.dll") do (
    echo   Processing: %%~nxf
    set "DEST=%PLUGINDIR%\%%~nxf"
    
    if exist "!DEST!" (
        del /f /q "!DEST!" 2>nul
        if exist "!DEST!" (
            echo     WARNING: Cannot delete, trying alternative method...
            takeown /f "!DEST!" >nul 2>&1
            icacls "!DEST!" /grant %username%:F >nul 2>&1
            del /f /q "!DEST!"
            if exist "!DEST!" (
                echo     FAILED: Could not delete %%~nxf
                set /a FAILED+=1
            ) else (
                copy /y "%%f" "!DEST!" >nul
                if !ERRORLEVEL! EQU 0 (
                    echo     OK: Updated %%~nxf
                    set /a UPDATED+=1
                ) else (
                    echo     FAILED: Could not copy %%~nxf
                    set /a FAILED+=1
                )
            )
        ) else (
            copy /y "%%f" "!DEST!" >nul
            if !ERRORLEVEL! EQU 0 (
                echo     OK: Updated %%~nxf
                set /a UPDATED+=1
            ) else (
                echo     FAILED: Could not copy %%~nxf
                set /a FAILED+=1
            )
        )
    ) else (
        copy /y "%%f" "!DEST!" >nul
        if !ERRORLEVEL! EQU 0 (
            echo     OK: Added %%~nxf
            set /a UPDATED+=1
        ) else (
            echo     FAILED: Could not copy %%~nxf
            set /a FAILED+=1
        )
    )
)

echo.
echo Step 3: Starting Directory Opus...
if exist "%DOPUSDIR%\Dopus.exe" (
    start "" "%DOPUSDIR%\Dopus.exe"
    echo Done.
) else (
    echo WARNING: Dopus.exe not found at %DOPUSDIR%\Dopus.exe
)

echo.
echo ========================================
echo Update Complete!
echo ========================================
echo   Updated: %UPDATED% plugins
echo   Failed:  %FAILED% plugins
echo.

if !FAILED! GTR 0 (
    echo NOTE: Some files failed. Try running this script as Administrator.
) else (
    echo All plugins updated successfully!
)
