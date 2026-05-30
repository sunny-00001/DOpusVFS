@echo off
setlocal enabledelayedexpansion

echo ========================================
echo DOpusRclone VFS Plugin Build Script v2.0
echo ========================================
echo.

set PLUGINDIR=%~dp0
set SRCDIR=%PLUGINDIR%src
set OUTDIR=%PLUGINDIR%..

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
echo Compiling DOpusRclone.dll...
echo.

set SOURCES=DOpusRclone.cpp RcloneClient.cpp
set INCLUDES=/I"%PLUGINDIR%include"
set DEFINES=/DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2
set CXXFLAGS=/nologo /W3 /O2 /EHsc /MT /LD /utf-8
set LIBS=Shell32.lib User32.lib Advapi32.lib Winhttp.lib Ole32.lib Crypt32.lib
set OUTFILE=%OUTDIR%\DOpusRclone.dll

cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:DOpusRclone.def %LIBS% resource.res

if %ERRORLEVEL% EQU 0 (
    del /q "%OUTDIR%\DOpusRclone.exp" 2>nul
    del /q "%OUTDIR%\DOpusRclone.lib" 2>nul
    
    del /q "%SRCDIR%\*.obj" 2>nul
    
    echo.
    echo ========================================
    echo Build successful!
    echo Output: %OUTFILE%
    echo ========================================
    
    for %%F in ("%OUTFILE%") do echo Size: %%~zF bytes
    echo.
) else (
    echo.
    echo ========================================
    echo Build FAILED!
    echo ========================================
)

cd /d "%PLUGINDIR%"
endlocal