@echo off
REM Analyze a local pulled log folder
setlocal
cd /d "%~dp0\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_analyze_logs.ps1" %*
set ERR=%ERRORLEVEL%
echo.
pause
exit /b %ERR%
