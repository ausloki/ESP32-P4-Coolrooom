@echo off
REM Download SD logs from the coolroom controller
setlocal
cd /d "%~dp0\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_pull_logs.ps1" %*
set ERR=%ERRORLEVEL%
echo.
pause
exit /b %ERR%
