@echo off
echo Compiling Logger Multi-threading Test...

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

cl.exe /nologo /W3 /EHsc /MT /utf-8 /DUNICODE /D_UNICODE test_logger_multithread.cpp Logger.cpp /Fe"test_logger_multithread.exe" /link Shell32.lib User32.lib Advapi32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Compilation successful!
    echo Running test...
    echo.
    test_logger_multithread.exe
) else (
    echo Compilation failed!
)

del /q *.obj 2>nul
