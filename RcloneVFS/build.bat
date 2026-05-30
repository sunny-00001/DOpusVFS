@echo off
setlocal enabledelayedexpansion

echo ========================================
echo RcloneVFS Plugin Build Script v2.2
echo ========================================
echo.

set SRCDIR=%~dp0
set OUTDIR=%SRCDIR%..

echo Setting up Visual Studio environment...
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

if %ERRORLEVEL% NEQ 0 (
    echo Failed to set up Visual Studio environment
    exit /b 1
)

echo.
echo Compiling resource file...
echo.

cd /d "%SRCDIR%"

rc.exe /nologo resource.rc >nul 2>&1

echo.
echo Compiling RcloneVFS.dll...
echo.

set SOURCES=RcloneVFS.cpp RcloneClient.cpp PathParser.cpp RcloneCache.cpp RcloneFeatures.cpp ColumnManager.cpp MenuManager.cpp DaemonManager.cpp ConfigManager.cpp ShareDialog.cpp VersionsDialog.cpp SyncDialog.cpp CheckDialog.cpp JobDialog.cpp TrashDialog.cpp Logger.cpp ThreadPool.cpp DebounceRefresh.cpp BatchOperationManager.cpp
set INCLUDES=/I"headers"
set DEFINES=/DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2
set CXXFLAGS=/nologo /W3 /O2 /EHsc /MT /LD /utf-8
set LIBS=Shell32.lib User32.lib Advapi32.lib Winhttp.lib Crypt32.lib Shlwapi.lib Gdi32.lib
set OUTFILE=%OUTDIR%\RcloneVFS.dll

cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:RcloneVFS.def %LIBS% resource.res

if %ERRORLEVEL% EQU 0 (
    del /q "%OUTDIR%\RcloneVFS.exp" 2>nul
    del /q "%OUTDIR%\RcloneVFS.lib" 2>nul
    
    echo.
    echo ========================================
    echo Build successful!
    echo Output: %OUTFILE%
    echo ========================================
    
    for %%F in ("%OUTFILE%") do echo Size: %%~zF bytes
    echo.
    
    echo Calling update-dopus-plugins.bat...
    echo.
    call "%OUTDIR%\update-dopus-plugins.bat"
) else (
    echo.
    echo ========================================
    echo Build FAILED!
    echo ========================================
)

cd /d "%SRCDIR%"
endlocal
