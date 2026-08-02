#requires -Version 5.1
# Analyse coolroom logs / recommend settings (Windows).
# Usage:
#   .\tools\analyse_logs_tune_settings.ps1 --log-dir tools\testdata\log_tune_30d
#   .\tools\analyse_logs_tune_settings.ps1 --host 192.168.37.237

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'log_tools_env.ps1')
Set-Location $env:COOLROOM_REPO_ROOT

& $env:COOLROOM_LOG_TOOLS_PYTHON (Join-Path $env:COOLROOM_TOOLS_DIR 'analyse_logs_tune_settings.py') @args
exit $LASTEXITCODE
