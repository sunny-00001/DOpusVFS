@echo off
setlocal EnableDelayedExpansion

echo Building ProcessVFS Plugin for Directory Opus
echo ==============================================

set SRCDIR=%~dp0
set OUTDIR=%SRCDIR%.

set VCVARSALL=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat

if exist "%VCVARSALL%" (
    echo Setting up Visual Studio environment...
    call "%VCVARSALL%" x64
) else (
    echo WARNING: vcvarsall.bat not found
    echo Checking if cl.exe is already in PATH...
    where cl.exe
    if errorlevel 1 (
        echo ERROR: Visual Studio compiler (cl.exe) not found
        echo Please install Visual Studio Build Tools or run from Developer Command Prompt
        exit /b 1
    )
)

echo.
echo Output directory: %OUTDIR%
echo Compiling ProcessVFS.dll...

rc /fo resource.res resource.rc
if errorlevel 1 (
    echo Resource compilation failed!
    exit /b 1
)

cl.exe /MT /O2 /EHsc /std:c++17 /utf-8 /I "." /I "include" /LD src\ProcessVFS.cpp resource.res /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2 /link /DEF:src\ProcessVFS.def Advapi32.lib Shell32.lib Comctl32.lib Psapi.lib /OUT:"%OUTDIR%\ProcessVFS.dll"

if errorlevel 1 (
    echo.
    echo =======================================
    echo Build failed! Please check the error messages above.
    echo =======================================
) else (
    echo.
    echo =======================================
    echo Build successful!
    echo Output: %OUTDIR%\ProcessVFS.dll
    echo =======================================
    if exist "%OUTDIR%\ProcessVFS.dll" (
        echo.
        echo File size:
        for %%F in ("%OUTDIR%\ProcessVFS.dll") do echo   %%~zF bytes
    )
)

if exist resource.res del resource.res

endlocal
