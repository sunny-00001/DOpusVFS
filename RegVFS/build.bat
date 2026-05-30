@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Registry VFS Plugin Build Script
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
echo Compiling RegistryVFS.dll...
echo.

set SOURCES=src\RegistryVFS.cpp
set INCLUDES=/I"include" /I"."
set DEFINES=/DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2
set CXXFLAGS=/nologo /W4 /O2 /EHsc /MD /LD /utf-8
set LIBS=advapi32.lib Shell32.lib Comctl32.lib Comdlg32.lib Ole32.lib User32.lib Gdi32.lib
set OUTFILE=%OUTDIR%\RegistryVFS.dll

if defined RESFILE (
    cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:src\RegistryVFS.def %LIBS% %RESFILE%
) else (
    cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:src\RegistryVFS.def %LIBS%
)

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build successful!
    echo Output: %OUTFILE%
    echo ========================================
    
    echo.
    echo Cleaning up temporary files...
    if exist "*.obj" del /Q "*.obj"
    if exist "*.exp" del /Q "*.exp"
    if exist "*.lib" del /Q "*.lib"
    if exist "*.res" del /Q "*.res"
    if exist "src\*.obj" del /Q "src\*.obj"
    if exist "src\*.res" del /Q "src\*.res"
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

endlocal
