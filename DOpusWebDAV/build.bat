@echo off
setlocal enabledelayedexpansion

echo Building DOpusWebDAV Plugin for Directory Opus
echo ==============================================

set SRCDIR=%~dp0
set ROOTDIR=%SRCDIR%..
set OUTDIR=%ROOTDIR%

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

echo Compiling resource file...
rc.exe /nologo resource.rc
if %ERRORLEVEL% NEQ 0 (
    echo Resource compilation failed
    set RESFILE=
) else (
    set RESFILE=resource.res
)

echo Compiling DOpusWebDAV.dll...

cl.exe /MT /O2 /EHsc /std:c++17 /utf-8 /LD DOpusWebDAV.cpp WebDAVClient.cpp pugixml.cpp %RESFILE% /I "headers" /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2 /link /DEF:DOpusWebDAV.def /NOIMPLIB Winhttp.lib Crypt32.lib Credui.lib Shell32.lib /OUT:"%OUTDIR%\DOpusWebDAV.dll"

if errorlevel 1 (
    echo.
    echo ========================================
    echo Build failed! Please check the error messages above.
    echo ========================================
    REM 即使失败也尝试清理临时文件
    if exist "*.obj" del /Q "*.obj"
    if exist "*.exp" del /Q "*.exp"
    if exist "*.lib" del /Q "*.lib"
    if exist "*.res" del /Q "*.res"
) else (
    echo.
    echo ========================================
    echo Build successful!
    echo Output: %OUTDIR%\DOpusWebDAV.dll
    echo ========================================
    
    REM 清理临时文件
    echo.
    echo Cleaning up temporary files...
    if exist "*.obj" del /Q "*.obj"
    if exist "*.exp" del /Q "*.exp"
    if exist "*.lib" del /Q "*.lib"
    if exist "*.res" del /Q "*.res"
    echo Temporary files cleaned up.
    
    if exist "%OUTDIR%\DOpusWebDAV.dll" (
        echo.
        echo File size:
        for %%F in ("%OUTDIR%\DOpusWebDAV.dll") do echo   %%~zF bytes
        
        echo.
        echo Automatically updating DOpus plugins...
        if exist "%ROOTDIR%\update-dopus-plugins.bat" (
            call "%ROOTDIR%\update-dopus-plugins.bat"
        ) else (
            echo update-dopus-plugins.bat not found, skipping auto-update
        )
    )
)

endlocal
