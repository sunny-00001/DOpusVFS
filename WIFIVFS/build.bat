@echo off
setlocal enabledelayedexpansion

echo ========================================
echo WIFIVFS Plugin Build Script v1.0
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

if exist resource_utf16.rc (
    rc.exe /nologo resource_utf16.rc
    if %ERRORLEVEL% NEQ 0 (
        echo Resource compilation failed, continuing without resources...
        set RESFILE=
    ) else (
        echo Resource compiled successfully.
        set RESFILE=resource_utf16.res
    )
) else (
    rc.exe /nologo resource.rc
    if %ERRORLEVEL% NEQ 0 (
        echo Resource compilation failed, continuing without resources...
        set RESFILE=
    ) else (
        echo Resource compiled successfully.
        set RESFILE=resource.res
    )
)

echo.
echo Compiling WIFIVFS.dll...
echo.

set SOURCES=src/WIFIVFS.cpp
set INCLUDES=/I"include" /I"."
set DEFINES=/DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2 /D_CRT_SECURE_NO_WARNINGS
set CXXFLAGS=/nologo /W3 /O2 /EHsc /MT /LD /utf-8
set LIBS=Shell32.lib User32.lib Advapi32.lib Comctl32.lib
set OUTFILE=%OUTDIR%\WIFIVFS.dll

if defined RESFILE (
    cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:WIFIVFS.def %LIBS% %RESFILE%
) else (
    cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:WIFIVFS.def %LIBS%
)

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build successful!
    echo Output: %OUTFILE%
    echo ========================================
    
    echo.
    echo Cleaning up temporary files...
    if exist "%OUTDIR%\WIFIVFS.lib" del /f "%OUTDIR%\WIFIVFS.lib"
    if exist "%OUTDIR%\WIFIVFS.exp" del /f "%OUTDIR%\WIFIVFS.exp"
    if exist "%SRCDIR%\WIFIVFS.lib" del /f "%SRCDIR%\WIFIVFS.lib"
    if exist "%SRCDIR%\WIFIVFS.exp" del /f "%SRCDIR%\WIFIVFS.exp"
    if exist "%SRCDIR%\*.obj" del /f "%SRCDIR%\*.obj"
    if exist "%SRCDIR%\*.res" del /f "%SRCDIR%\*.res"
    if exist "%SRCDIR%\resource.res" del /f "%SRCDIR%\resource.res"
    echo Cleanup complete.
    
    if exist "%OUTFILE%" (
        echo.
        echo File size:
        for %%F in ("%OUTFILE%") do echo   %%~zF bytes
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
    echo Build FAILED!
    echo ========================================
)

cd /d "%SRCDIR%"
endlocal
