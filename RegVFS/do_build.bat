@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d D:\VFS\RegVFS
if exist build rmdir /s /q build
cmake -G Ninja -B build -S . -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo CMAKE CONFIGURE FAILED
    exit /b %ERRORLEVEL%
)
cmake --build build --config Release
if %ERRORLEVEL% NEQ 0 (
    echo CMAKE BUILD FAILED
    exit /b %ERRORLEVEL%
)
copy /Y build\RegistryVFS.dll ..\RegistryVFS.dll
echo BUILD SUCCESS
dir ..\RegistryVFS.dll
