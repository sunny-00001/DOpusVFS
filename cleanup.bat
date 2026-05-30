@echo off
echo ========================================
echo Cleaning up temporary files
echo ========================================
echo.
echo Deleting *.lib, *.exp, *.obj, *.res files...
del /s /q *.lib >nul 2>&1
del /s /q *.exp >nul 2>&1
del /s /q *.obj >nul 2>&1
del /s /q *.res >nul 2>&1
echo.
echo Cleanup complete!
echo.
pause
