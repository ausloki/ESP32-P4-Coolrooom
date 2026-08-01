#requires -Version 5.1
param(
  [string]$HostAddress = '',
  [int]$Days = 14,
  [switch]$ListOnly
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'log_tools_env.ps1')
Set-Location $env:COOLROOM_REPO_ROOT

if ([string]::IsNullOrWhiteSpace($HostAddress)) {
  $HostAddress = Read-Host 'Controller host/IP [192.168.37.237]'
  if ([string]::IsNullOrWhiteSpace($HostAddress)) { $HostAddress = '192.168.37.237' }
}

$script = Join-Path $env:COOLROOM_TOOLS_DIR 'pull_controller_logs.py'
if ($ListOnly) {
  & $env:COOLROOM_LOG_TOOLS_PYTHON $script --host $HostAddress --list-only
} else {
  & $env:COOLROOM_LOG_TOOLS_PYTHON $script --host $HostAddress --days $Days
}
