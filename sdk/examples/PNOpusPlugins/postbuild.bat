@echo off

rem arg 1 = Win32/x64
rem arg 2 = target


rem on Leo's system, copy the target to the Opus program folder


rem Don't want to do this anymore (but may want to again at some point)
goto END

if not "%USERNAME%" == "Leo" goto END
if not "%USERDOMAIN%" == "Lateralus" goto END

"C:\program files\gpsoftware\directory opus\dopusrt.exe" /flushplugins

set I="0"

:LOOP

if "%1" == "Win32" (
	copy %2 "c:\Program Files\GPSoftware\Directory Opus\Viewers"
	if not errorlevel 1 goto END
) else (
	copy %2 "c:\Program Files\GPSoftware\Directory Opus\Viewers64"
	if not errorlevel 1 goto END
)

set /A I+=1

if %I% LSS 10 goto LOOP

rem If the copy failed, delete the source file so that the IDE will actually do something if we try again.
del %2
exit /B 1

:END
