@echo off
REM Pull + analyze: recommend coolroom settings from controller logs
setlocal
cd /d "%~dp0\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_recommend_settings.ps1" %*
set ERR=%ERRORLEVEL%
echo.
pause
exit /b %ERR%
