#requires -Version 5.1
<#
.SYNOPSIS
  Interactive menu for coolroom log pull / settings recommend (Windows).

.DESCRIPTION
  Ensures tools\.venv-log-tuning, then runs pull / analyze / recommend scripts.
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
  $days = Read-HostDefault 'Days of dated temp logs (0 = all)' '14'
  & $Py (Join-Path $Tools 'recommend_settings.py') --host $h --days $days
}

function Invoke-Pull {
  $h = Read-HostDefault 'Controller host/IP' $DefaultHost
  $days = Read-HostDefault 'Days of dated temp logs (0 = all)' '14'
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
  $h = Read-HostDefault 'Controller host for live settings (blank to skip)' $DefaultHost
  $args = @((Join-Path $Tools 'analyze_coolroom_logs.py'), $dir)
  if (-not [string]::IsNullOrWhiteSpace($h)) {
    $args += @('--host', $h)
  }
  & $Py @args
}

Write-Host ''
Write-Host '=== ESP32-P4 Coolroom — Log tools (Windows) ==='
Write-Host "Python: $Py"
Write-Host "Repo:   $Repo"
Write-Host ''
Write-Host '  1) Recommend settings (pull logs + analyze)'
Write-Host '  2) Pull logs only'
Write-Host '  3) List SD files on controller'
Write-Host '  4) Analyze existing local log folder'
Write-Host '  Q) Quit'
Write-Host ''

$choice = Read-Host 'Choose'
switch ($choice.Trim().ToUpperInvariant()) {
  '1' { Invoke-Recommend }
  '2' { Invoke-Pull }
  '3' { Invoke-List }
  '4' { Invoke-Analyze }
  'Q' { exit 0 }
  default {
    Write-Host "Unknown choice: $choice"
    exit 1
  }
}

Write-Host ''
Write-Host 'Done. Press Enter to close.'
[void][System.Console]::ReadLine()
