@echo off
setlocal enabledelayedexpansion

REM Set Visual Studio 2022 environment for x64
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

echo.
echo ========================================
echo Compiling DOpusRclone.dll
echo ========================================
echo.

REM Compile the DLL
cl /nologo /LD /EHsc /O2 /DNDEBUG /DWIN32 /D_WINDOWS /D_UNICODE /DUNICODE ^
 /ID:\VFS\dopus-rclone-vfs-main ^
 /ID:\VFS\dopus-rclone-vfs-main\headers ^
 /IC:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um ^
 /IC:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared ^
 /IC:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt ^
 /IC:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\include ^
 D:\VFS\dopus-rclone-vfs-main\DOpusRclone.cpp ^
 D:\VFS\dopus-rclone-vfs-main\RcloneClient.cpp ^
 /link ^
 /DEF:D:\VFS\dopus-rclone-vfs-main\DOpusRclone.def ^
 User32.lib Shell32.lib Advapi32.lib Winhttp.lib Ole32.lib ^
 /OUT:D:\VFS\DOpusRclone.dll

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Compilation failed with error code %ERRORLEVEL%
    goto :end
)

echo.
echo [SUCCESS] DOpusRclone.dll compiled successfully!
echo Output: D:\VFS\DOpusRclone.dll
echo.

: end
pause
