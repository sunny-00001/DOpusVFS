@echo off
setlocal enabledelayedexpansion

echo Building GitHubVFS Plugin for Directory Opus
echo ==============================================

set SRCDIR=%~dp0
set OUTDIR=%SRCDIR%..

set VCVARSALL=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat

if exist "%VCVARSALL%" (
    echo Setting up Visual Studio environment...
    call "%VCVARSALL%" x64
) else (
    echo WARNING: vcvarsall.bat not found
    echo Checking if cl.exe is already in PATH...
    where cl.exe
    if errorlevel 1 (
        echo ERROR: Visual Studio compiler ^(cl.exe^) not found
        echo Please install Visual Studio Build Tools or run from Developer Command Prompt
        pause
        exit /b 1
    )
)

echo.
echo Output directory: %OUTDIR%
echo Compiling resources...
rc.exe /r /fo resource.res src\resource.rc
if errorlevel 1 (
    echo.
    echo ========================================
    echo Resource compilation failed!
    echo ========================================
    goto :end
)
echo Compiling GitHubVFS.dll...

cl.exe /MT /O2 /EHsc /std:c++17 /utf-8 /I "." /I "include" /I "src" /LD src\GitHubVFS.cpp src\GitHubClient.cpp /DUNICODE /D_UNICODE /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2 /D_WIN32_WINNT=0x0A00 /link /DEF:src\GitHubVFS.def /MAP Advapi32.lib Crypt32.lib Shell32.lib Comctl32.lib Winhttp.lib User32.lib Gdi32.lib Comdlg32.lib resource.res /OUT:"%OUTDIR%\GitHubVFS.dll"

if errorlevel 1 (
    echo.
    echo ========================================
    echo Build failed! Please check the error messages above.
    echo ========================================
) else (
    echo.
    echo ========================================
    echo Build successful!
    echo Output: %OUTDIR%\GitHubVFS.dll
    echo ========================================
    if exist "%OUTDIR%\GitHubVFS.dll" (
        echo.
        echo File size:
        for %%F in ("%OUTDIR%\GitHubVFS.dll") do echo   %%~zF bytes

        echo.
        echo Cleaning up temporary files...
        if exist "%OUTDIR%\GitHubVFS.exp" del "%OUTDIR%\GitHubVFS.exp"
        if exist "%OUTDIR%\GitHubVFS.lib" del "%OUTDIR%\GitHubVFS.lib"
        echo Cleanup done.

        echo.
        echo ========================================
        echo Deploying to Directory Opus...
        echo ========================================
        if exist "%OUTDIR%\update-dopus-plugins.bat" (
            call "%OUTDIR%\update-dopus-plugins.bat"
        ) else (
            echo WARNING: update-dopus-plugins.bat not found at %OUTDIR%
            echo Please manually copy GitHubVFS.dll to your DOpus VFSPlugins directory
        )
    )
)

if exist resource.res del resource.res

:end
endlocal
