@echo off
setlocal enabledelayedexpansion

echo Building TaskSchedulerVFS Plugin for Directory Opus
echo ====================================================

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
echo Working directory: %SRCDIR%
cd /d "%SRCDIR%"
echo Compiling resources...

rc.exe /I "." /I "include" resource.rc
if errorlevel 1 (
    echo Resource compilation failed!
    pause
    exit /b 1
)

echo Compiling TaskSchedulerVFS.dll...

cl.exe /MT /O2 /EHsc /std:c++17 /utf-8 /I "." /I "include" /LD src\TaskSchedulerVFS.cpp resource.res /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2 /link /DEF:src\TaskSchedulerVFS.def Advapi32.lib Shell32.lib Comctl32.lib Ole32.lib OleAut32.lib Gdi32.lib Comdlg32.lib Taskschd.lib /OUT:"%OUTDIR%\TaskSchedulerVFS.dll"

if errorlevel 1 (
    echo.
    echo ========================================
    echo Build failed! Please check the error messages above.
    echo ========================================
    goto Cleanup
) else (
    echo.
    echo ========================================
    echo Build successful!
    echo Output: %OUTDIR%\TaskSchedulerVFS.dll
    echo ========================================
    if exist "%OUTDIR%\TaskSchedulerVFS.dll" (
        echo.
        echo File size:
        for %%F in ("%OUTDIR%\TaskSchedulerVFS.dll") do echo   %%~zF bytes
    )
)

:Cleanup
echo.
echo Cleaning up temporary files...
if exist resource.res del /F resource.res
if exist "src\TaskSchedulerVFS.obj" del /F "src\TaskSchedulerVFS.obj"
if exist TaskSchedulerVFS.exp del /F TaskSchedulerVFS.exp
if exist TaskSchedulerVFS.lib del /F TaskSchedulerVFS.lib
if exist "%OUTDIR%\TaskSchedulerVFS.exp" del /F "%OUTDIR%\TaskSchedulerVFS.exp"
if exist "%OUTDIR%\TaskSchedulerVFS.lib" del /F "%OUTDIR%\TaskSchedulerVFS.lib"
echo Done.

endlocal
pause
