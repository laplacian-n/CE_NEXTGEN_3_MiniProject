@echo off
rem Double-click to check Node-RED (runs check_nodered.ps1 without changing the PowerShell policy)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0check_nodered.ps1" %*
set rc=%errorlevel%
echo.
pause
exit /b %rc%
