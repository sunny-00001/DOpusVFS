@echo off
setlocal enabledelayedexpansion

echo ========================================
echo IPFSVFS Plugin Build Script v1.0
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

rc.exe /nologo src/resource.rc

if %ERRORLEVEL% NEQ 0 (
    echo Resource compilation failed, continuing without resources...
    set RESFILE=
) else (
    echo Resource compiled successfully.
    set RESFILE=src/resource.res
)

echo.
echo Compiling IPFSVFS.dll...
echo.

set SOURCES=src/IPFSVFS.cpp src/IPFSClient.cpp
set INCLUDES=/I"include" /I"src"
set DEFINES=/DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2
set CXXFLAGS=/nologo /W3 /O2 /EHsc /MT /LD /utf-8
set LIBS=Shell32.lib User32.lib Advapi32.lib Comctl32.lib Winhttp.lib Ole32.lib
set OUTFILE=%OUTDIR%\IPFSVFS.dll

if defined RESFILE (
    cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:src/IPFSVFS.def /NOIMPLIB %LIBS% %RESFILE%
) else (
    cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:src/IPFSVFS.def /NOIMPLIB %LIBS%
)

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build successful!
    echo Output: %OUTFILE%
    echo ========================================
    
    REM 清理临时文件
    echo.
    echo Cleaning up temporary files...
    if exist "%SRCDIR%\*.obj" del /Q "%SRCDIR%\*.obj"
    if exist "%SRCDIR%\*.exp" del /Q "%SRCDIR%\*.exp"
    if exist "%SRCDIR%\*.lib" del /Q "%SRCDIR%\*.lib"
    if exist "%SRCDIR%\*.res" del /Q "%SRCDIR%\*.res"
    if exist "%OUTDIR%\IPFSVFS.exp" del /Q "%OUTDIR%\IPFSVFS.exp"
    if exist "%OUTDIR%\IPFSVFS.lib" del /Q "%OUTDIR%\IPFSVFS.lib"
    if exist "src\*.obj" del /Q "src\*.obj"
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
    echo Build failed!
    echo ========================================
    REM 即使失败也尝试清理临时文件
    if exist "%SRCDIR%\*.obj" del /Q "%SRCDIR%\*.obj"
    if exist "%SRCDIR%\*.exp" del /Q "%SRCDIR%\*.exp"
    if exist "%SRCDIR%\*.lib" del /Q "%SRCDIR%\*.lib"
    if exist "%SRCDIR%\*.res" del /Q "%SRCDIR%\*.res"
    if exist "%OUTDIR%\IPFSVFS.exp" del /Q "%OUTDIR%\IPFSVFS.exp"
    if exist "%OUTDIR%\IPFSVFS.lib" del /Q "%OUTDIR%\IPFSVFS.lib"
    if exist "src\*.obj" del /Q "src\*.obj"
)

cd /d "%SRCDIR%"
endlocal
