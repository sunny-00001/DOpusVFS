@echo off
setlocal enabledelayedexpansion

echo ========================================
echo EnvVFS Plugin Build Script v1.0
echo ========================================
echo.

set SRCDIR=%~dp0
set ROOTDIR=%SRCDIR%..
set OUTDIR=%ROOTDIR%

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo Setting up Visual Studio environment...
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

if %ERRORLEVEL% NEQ 0 (
    echo Failed to set up Visual Studio environment
    exit /b 1
)

echo.
echo Compiling resource file...
echo.

cd /d "%SRCDIR%"

rc.exe /nologo resource.rc

if %ERRORLEVEL% NEQ 0 (
    echo Resource compilation failed, continuing without resources...
    set RESFILE=
) else (
    echo Resource compiled successfully.
    set RESFILE=resource.res
)

echo.
echo Compiling EnvVFS.dll...
echo.

set SOURCES=src/EnvVFS.cpp
set INCLUDES=/I"include" /I"."
set DEFINES=/DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2
set CXXFLAGS=/nologo /W3 /O2 /EHsc /MT /LD /utf-8
set LIBS=Shell32.lib User32.lib Advapi32.lib Comctl32.lib Gdi32.lib Comdlg32.lib
set OUTFILE=%OUTDIR%\EnvVFS.dll

if defined RESFILE (
    cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:EnvVFS.def %LIBS% %RESFILE%
) else (
    cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:EnvVFS.def %LIBS%
)

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build successful
    echo Output: %OUTFILE%
    echo ========================================
    
    if exist "%OUTFILE%" (
        echo.
        echo File size:
        for %%F in ("%OUTFILE%") do echo   %%~zF bytes
        
        REM Clean up intermediate files
        echo.
        echo Cleaning up intermediate files...
        if exist "%OUTDIR%\EnvVFS.lib" del "%OUTDIR%\EnvVFS.lib"
        if exist "%OUTDIR%\EnvVFS.exp" del "%OUTDIR%\EnvVFS.exp"
        if exist "%SRCDIR%\EnvVFS.obj" del "%SRCDIR%\EnvVFS.obj"
        if exist "%SRCDIR%\resource.res" del "%SRCDIR%\resource.res"
        echo Cleanup complete. Only EnvVFS.dll remains.
        
        echo.
        echo Automatically updating DOpus plugins...
        if exist "%ROOTDIR%\update-dopus-plugins.bat" (
            call "%ROOTDIR%\update-dopus-plugins.bat"
        ) else (
            echo update-dopus-plugins.bat not found, skipping auto-update
        )
    )
) else (
    echo.
    echo ========================================
    echo Build FAILED
    echo ========================================
)

cd /d "%SRCDIR%"
endlocal
