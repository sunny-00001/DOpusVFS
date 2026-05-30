@echo off
echo Checking RcloneVFS.dll exports...
echo.

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

dumpbin /exports "d:\VFS\RcloneVFS\build\Release\RcloneVFS.dll"

pause
