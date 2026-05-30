@echo off
REM Compile DOpusRclone VFS Plugin
REM Run this file in Visual Studio Developer Command Prompt or double-click it

REM Check if cl.exe is available
where cl.exe >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [INFO] Visual Studio environment not detected. Setting up...
    if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
        echo [ERROR] Visual Studio 2022 not found!
        echo Please run this script from Developer Command Prompt for VS 2022
        pause
        exit /b 1
    )
)

echo.
echo ========================================
echo Compiling DOpusRclone.dll
echo ========================================
echo.

REM Set paths
set SRC_DIR=D:\VFS\dopus-rclone-vfs-main
set OUTPUT_DIR=D:\VFS

REM Windows SDK version
set WINSDK_VER=10.0.26100.0

REM Set INCLUDE paths
set INCLUDE=%INCLUDE%;%SRC_DIR%;%SRC_DIR%\headers
set INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\Include\%WINSDK_VER%\um
set INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\Include\%WINSDK_VER%\shared
set INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\Include\%WINSDK_VER%\ucrt
set INCLUDE=%INCLUDE%;C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\include

REM Set LIB paths
set LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\Lib\%WINSDK_VER%\um\x64
set LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\Lib\%WINSDK_VER%\ucrt\x64
set LIB=%LIB%;C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\lib\x64

REM Compile
cl /nologo /LD /EHsc /O2 /DNDEBUG /DWIN32 /D_WINDOWS /D_UNICODE /DUNICODE ^
 %SRC_DIR%\DOpusRclone.cpp ^
 %SRC_DIR%\RcloneClient.cpp ^
 /link ^
 /DEF:%SRC_DIR%\DOpusRclone.def ^
 User32.lib Shell32.lib Advapi32.lib Winhttp.lib Ole32.lib ^
 /OUT:%OUTPUT_DIR%\DOpusRclone.dll

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Compilation failed with error code %ERRORLEVEL%
    goto :end
)

echo.
echo [SUCCESS] DOpusRclone.dll compiled successfully!
echo Output: %OUTPUT_DIR%\DOpusRclone.dll
echo.

: end

