@echo off
setlocal enabledelayedexpansion

echo ========================================
echo DOpusRclone VFS Plugin Build Script
echo Output to: d:\VFS\Vfs
echo ========================================
echo.

set SRCDIR=%~dp0
set OUTDIR=d:\VFS\Vfs

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo Setting up Visual Studio environment...
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

if %ERRORLEVEL% NEQ 0 (
    echo Failed to set up Visual Studio environment
    exit /b 1
)

echo.
echo Compiling DOpusRclone.dll...
echo.

set SOURCES=DOpusRclone.cpp RcloneClient.cpp
set INCLUDES=/I"headers"
set DEFINES=/DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER
set CXXFLAGS=/nologo /W3 /O2 /EHsc /MT /LD
set LIBS=Shell32.lib User32.lib Advapi32.lib Winhttp.lib Ole32.lib Crypt32.lib
set OUTFILE=%OUTDIR%\DOpusRclone.dll

cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:DOpusRclone.def %LIBS%

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build successful!
    echo Output: %OUTFILE%
    echo ========================================
    
    if exist "%OUTFILE%" (
        echo.
        echo File size:
        for %%F in ("%OUTFILE%") do echo   %%~zF bytes
        echo.
        echo To install:
        echo 1. Copy %OUTFILE% to your Directory Opus VFSPlugins directory
        echo 2. Restart Directory Opus
        echo 3. Type rclone:// in the address bar
    )
) else (
    echo.
    echo ========================================
    echo Build FAILED!
    echo ========================================
)

cd /d "%SRCDIR%"
endlocal
