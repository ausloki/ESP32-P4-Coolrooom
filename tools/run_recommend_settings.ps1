#requires -Version 5.1
param(
  [string]$HostAddress = '',
  [int]$Days = 14
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'log_tools_env.ps1')
Set-Location $env:COOLROOM_REPO_ROOT

if ([string]::IsNullOrWhiteSpace($HostAddress)) {
  $HostAddress = Read-Host 'Controller host/IP [192.168.37.237]'
  if ([string]::IsNullOrWhiteSpace($HostAddress)) { $HostAddress = '192.168.37.237' }
}

& $env:COOLROOM_LOG_TOOLS_PYTHON `
  (Join-Path $env:COOLROOM_TOOLS_DIR 'recommend_settings.py') `
  --host $HostAddress `
  --days $Days
