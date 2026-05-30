@echo off
setlocal enabledelayedexpansion
set SRCDIR=%~dp0
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
cd /d %SRCDIR%
cl /nologo test_icon.c /Fe:test_icon.exe /link shell32.lib user32.lib
if %ERRORLEVEL% EQU 0 (
    echo Running test...
    test_icon.exe
) else (
    echo Compile failed
)
