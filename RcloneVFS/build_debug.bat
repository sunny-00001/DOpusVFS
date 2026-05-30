@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
cl.exe /nologo /O2 /MT debug_capture.c /Fe:debug_capture.exe
if %ERRORLEVEL% EQU 0 (
    echo Build debug_capture.exe successful
) else (
    echo Build failed
)
