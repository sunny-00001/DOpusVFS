@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
echo === Exports ===
dumpbin /exports "d:\VFS\RcloneVFS.dll" | findstr /i "VFS_"
echo.
echo === Resources ===
dumpbin /rawdata:2 "d:\VFS\RcloneVFS.dll" | findstr /i "icon" >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Icon resource found in DLL
) else (
    echo Checking for icon resource...
)
rc /d /fo nul "d:\VFS\RcloneVFS\resource.rc" >nul 2>&1
