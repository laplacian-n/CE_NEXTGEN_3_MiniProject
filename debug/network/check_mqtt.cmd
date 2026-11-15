@echo off
rem Double-click to check the MQTT connection (runs check_mqtt.ps1 without changing the PowerShell policy)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0check_mqtt.ps1" %*
set rc=%errorlevel%
echo.
pause
exit /b %rc%
