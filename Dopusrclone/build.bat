@echo off
setlocal enabledelayedexpansion

echo ========================================
echo DOpusRclone VFS Plugin Build Script
echo ========================================
echo.

set SRCDIR=%~dp0
set ROOTDIR=%SRCDIR%..
set OUTDIR=%ROOTDIR%

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo Setting up Visual Studio environment...
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

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
    echo Resource compilation failed
    set RESFILE=
) else (
    set RESFILE=resource.res
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

cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% %RESFILE% /Fe"%OUTFILE%" /link /DEF:DOpusRclone.def /NOIMPLIB %LIBS%

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build successful!
    echo Output: %OUTFILE%
    echo ========================================
    
    REM 清理临时文件
    echo.
    echo Cleaning up temporary files...
    if exist "*.obj" del /Q "*.obj"
    if exist "*.exp" del /Q "*.exp"
    if exist "*.lib" del /Q "*.lib"
    if exist "*.res" del /Q "*.res"
    echo Temporary files cleaned up.
    
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
    REM 即使失败也尝试清理临时文件
    if exist "*.obj" del /Q "*.obj"
    if exist "*.exp" del /Q "*.exp"
    if exist "*.lib" del /Q "*.lib"
    if exist "*.res" del /Q "*.res"
)

cd /d "%SRCDIR%"
endlocal
