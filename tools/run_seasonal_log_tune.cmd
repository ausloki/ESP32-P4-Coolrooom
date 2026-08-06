@echo off
REM Scheduled / one-shot seasonal log tune (recommend-only). Never --apply.
REM Usage: tools\run_seasonal_log_tune.cmd --host 192.168.37.237
REM        tools\run_seasonal_log_tune.cmd --log-dir tools\testdata\log_tune_30d --window last30 --min-days 0
setlocal
cd /d "%~dp0\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_seasonal_log_tune.ps1" %*
set ERR=%ERRORLEVEL%
exit /b %ERR%
