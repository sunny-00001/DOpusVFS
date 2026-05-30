@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d d:\VFS\RegVFS
if exist build rmdir /s /q build
cmake -G Ninja -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
copy /Y build\RegistryVFS.dll ..\RegistryVFS.dll
dir ..\RegistryVFS.dll
