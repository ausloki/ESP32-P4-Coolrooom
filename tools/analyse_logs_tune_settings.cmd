@echo off
REM Analyse coolroom logs / recommend settings (Windows cmd)
setlocal
cd /d "%~dp0\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0analyse_logs_tune_settings.ps1" %*
set ERR=%ERRORLEVEL%
echo.
pause
exit /b %ERR%
