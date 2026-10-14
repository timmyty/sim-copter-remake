@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-OptionalMusic.ps1" %*
set "music_result=%ERRORLEVEL%"
echo.
if not "%music_result%"=="0" echo Music installation did not complete. See the message above.
pause
exit /b %music_result%
