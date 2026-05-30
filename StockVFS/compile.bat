@echo off
cd /d "%~dp0\build"
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
echo.
echo Build complete. DLL deployed to D:\Dopus\VFSPlugins\StockVFS.dll
pause
