#requires -Version 5.1
# Scheduled / one-shot seasonal log tune (recommend-only).
# Never passes --apply. Writes dated HTML under logs/tune_reports/.
#
# Usage (PowerShell named params OR -- flags for .cmd parity):
#   .\tools\run_seasonal_log_tune.ps1 -HostAddress 192.168.37.237
#   .\tools\run_seasonal_log_tune.ps1 --host 192.168.37.237 --window picking
#   .\tools\run_seasonal_log_tune.ps1 --log-dir tools\testdata\log_tune_30d --window last30 --min-days 0
#
# Env: COOLROOM_HOST, COOLROOM_WINDOW, COOLROOM_REPORT_DIR, COOLROOM_MIN_DAYS, COOLROOM_LOG_DIR
# See tools/README_LOG_TUNING.md (Task Scheduler example).

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'log_tools_env.ps1')
Set-Location $env:COOLROOM_REPO_ROOT

$hostAddr = $env:COOLROOM_HOST
$logDir = $env:COOLROOM_LOG_DIR
$window = if ($env:COOLROOM_WINDOW) { $env:COOLROOM_WINDOW } else { 'last30' }
$reportDir = if ($env:COOLROOM_REPORT_DIR) { $env:COOLROOM_REPORT_DIR } else { 'logs/tune_reports' }
$minDays = if ($env:COOLROOM_MIN_DAYS) { $env:COOLROOM_MIN_DAYS } else { '30' }
$since = $null
$until = $null
$extra = New-Object System.Collections.Generic.List[string]

$argv = @($args)
$i = 0
while ($i -lt $argv.Count) {
    $a = [string]$argv[$i]
    switch -Regex ($a) {
        '^--host$' { $hostAddr = [string]$argv[$i + 1]; $i += 2; continue }
        '^-HostAddress$' { $hostAddr = [string]$argv[$i + 1]; $i += 2; continue }
        '^--log-dir$' { $logDir = [string]$argv[$i + 1]; $i += 2; continue }
        '^-LogDir$' { $logDir = [string]$argv[$i + 1]; $i += 2; continue }
        '^--window$' { $window = [string]$argv[$i + 1]; $i += 2; continue }
        '^-Window$' { $window = [string]$argv[$i + 1]; $i += 2; continue }
        '^--report-dir$' { $reportDir = [string]$argv[$i + 1]; $i += 2; continue }
        '^-ReportDir$' { $reportDir = [string]$argv[$i + 1]; $i += 2; continue }
        '^--min-days$' { $minDays = [string]$argv[$i + 1]; $i += 2; continue }
        '^-MinDays$' { $minDays = [string]$argv[$i + 1]; $i += 2; continue }
        '^--since$' { $since = [string]$argv[$i + 1]; $window = ''; $i += 2; continue }
        '^--until$' { $until = [string]$argv[$i + 1]; $window = ''; $i += 2; continue }
        '^-h$|^--help$' {
            Write-Host @'
run_seasonal_log_tune — recommend-only seasonal HTML under logs/tune_reports/
  --host IP | --log-dir PATH
  --window last30|picking   (or --since/--until YYYY-MM-DD)
  --min-days N  --report-dir DIR
'@
            exit 0
        }
        default { $extra.Add($a); $i += 1; continue }
    }
}

New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
$stamp = (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')
$reportPath = Join-Path $reportDir "tune_$stamp.html"

$pyArgs = [System.Collections.Generic.List[string]]::new()
$pyArgs.AddRange([string[]]@('--min-days', $minDays, '--report', $reportPath))
if ($since -and $until) {
    $pyArgs.AddRange([string[]]@('--since', $since, '--until', $until))
} elseif ($window) {
    $pyArgs.AddRange([string[]]@('--window', $window))
}
if ($hostAddr) {
    $pyArgs.AddRange([string[]]@('--host', $hostAddr))
} elseif ($logDir) {
    $pyArgs.AddRange([string[]]@('--log-dir', $logDir))
} else {
    Write-Error 'Provide --host IP or --log-dir PATH (or COOLROOM_HOST / COOLROOM_LOG_DIR)'
}

Write-Host "→ seasonal tune (recommend-only) report=$reportPath"
& $env:COOLROOM_LOG_TOOLS_PYTHON (Join-Path $env:COOLROOM_TOOLS_DIR 'analyse_logs_tune_settings.py') @($pyArgs.ToArray()) @($extra.ToArray())
exit $LASTEXITCODE
