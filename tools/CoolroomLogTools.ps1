#requires -Version 5.1
<#
.SYNOPSIS
  Interactive menu for coolroom log pull / settings recommend (Windows).

.DESCRIPTION
  Ensures tools\.venv-log-tuning, then runs pull / analyse / recommend scripts.
  Tweaks require ≥30 days of history in the selected window (see tools/README_LOG_TUNING.md).
#>

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'log_tools_env.ps1')

$Py = $env:COOLROOM_LOG_TOOLS_PYTHON
$Repo = $env:COOLROOM_REPO_ROOT
$Tools = $env:COOLROOM_TOOLS_DIR
Set-Location $Repo

$DefaultHost = '192.168.37.237'

function Read-HostDefault([string]$Prompt, [string]$Default) {
  $v = Read-Host "$Prompt [$Default]"
  if ([string]::IsNullOrWhiteSpace($v)) { return $Default }
  return $v.Trim()
}

function Invoke-Recommend {
  $h = Read-HostDefault 'Controller host/IP' $DefaultHost
  $days = Read-HostDefault 'Days of dated temp logs to pull (0 = all)' '0'
  & $Py (Join-Path $Tools 'analyse_logs_tune_settings.py') --host $h --days $days
}

function Invoke-Pull {
  $h = Read-HostDefault 'Controller host/IP' $DefaultHost
  $days = Read-HostDefault 'Days of dated temp logs (0 = all)' '0'
  & $Py (Join-Path $Tools 'pull_controller_logs.py') --host $h --days $days
}

function Invoke-List {
  $h = Read-HostDefault 'Controller host/IP' $DefaultHost
  & $Py (Join-Path $Tools 'pull_controller_logs.py') --host $h --list-only
}

function Invoke-Analyze {
  $latest = Get-ChildItem -Path (Join-Path $Repo 'logs\controller') -Directory -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
  $defaultDir = if ($latest) { $latest.FullName } else { (Join-Path $Repo 'logs\controller') }
  $dir = Read-HostDefault 'Log directory to analyze' $defaultDir
  $h = Read-Host 'Controller host for live settings (blank to skip)'
  if (-not [string]::IsNullOrWhiteSpace($h)) {
    & $Py (Join-Path $Tools 'analyse_logs_tune_settings.py') --log-dir $dir --settings-host $h.Trim()
  } else {
    & $Py (Join-Path $Tools 'analyse_logs_tune_settings.py') --log-dir $dir
  }
}

function Invoke-Seasonal {
  $h = Read-HostDefault 'Controller host/IP (blank = local --log-dir)' $DefaultHost
  $window = Read-HostDefault 'Window (last30|picking)' 'last30'
  $runner = Join-Path $Tools 'run_seasonal_log_tune.ps1'
  if (-not [string]::IsNullOrWhiteSpace($h)) {
    & $runner --host $h --window $window
  } else {
    $latest = Get-ChildItem -Path (Join-Path $Repo 'logs\controller') -Directory -ErrorAction SilentlyContinue |
      Sort-Object LastWriteTime -Descending |
      Select-Object -First 1
    $defaultDir = if ($latest) { $latest.FullName } else { (Join-Path $Repo 'logs\controller') }
    $dir = Read-HostDefault 'Log directory' $defaultDir
    & $runner --log-dir $dir --window $window
  }
}

Write-Host ''
Write-Host '=== ESP32-P4 Coolroom — Log tools (Windows) ==='
Write-Host "Python: $Py"
Write-Host "Repo:   $Repo"
Write-Host 'Note:   Tweaks require ≥30 days of history in the selected window.'
Write-Host 'Docs:   tools/README_LOG_TUNING.md'
Write-Host ''
Write-Host '  1) Analyse + recommend (pull from host, ≥30-day gate)'
Write-Host '  2) Pull logs only'
Write-Host '  3) List SD files on controller'
Write-Host '  4) Analyse existing local log folder (≥30-day gate)'
Write-Host '  5) Seasonal / windowed recommend-only → logs/tune_reports/'
Write-Host '  Q) Quit'
Write-Host ''

$choice = Read-Host 'Choose'
switch ($choice.Trim().ToUpperInvariant()) {
  '1' { Invoke-Recommend }
  '2' { Invoke-Pull }
  '3' { Invoke-List }
  '4' { Invoke-Analyze }
  '5' { Invoke-Seasonal }
  'Q' { exit 0 }
  default {
    Write-Host "Unknown choice: $choice"
    exit 1
  }
}

Write-Host ''
Write-Host 'Done. Press Enter to close.'
[void][System.Console]::ReadLine()
