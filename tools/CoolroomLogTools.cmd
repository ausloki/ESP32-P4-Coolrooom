@echo off
REM Double-click or run from cmd: coolroom log tools menu (Windows)
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0CoolroomLogTools.ps1" %*
set ERR=%ERRORLEVEL%
if %ERR% neq 0 (
  echo.
  echo Exit code %ERR%
  pause
)
exit /b %ERR%
