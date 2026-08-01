#requires -Version 5.1
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'log_tools_env.ps1')
Set-Location $env:COOLROOM_REPO_ROOT

$latest = Get-ChildItem -Path (Join-Path $env:COOLROOM_REPO_ROOT 'logs\controller') -Directory -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1

if ($args.Count -ge 1) {
  $dir = $args[0]
} else {
  $default = if ($latest) { $latest.FullName } else { Join-Path $env:COOLROOM_REPO_ROOT 'logs\controller' }
  $dir = Read-Host "Log directory [$default]"
  if ([string]::IsNullOrWhiteSpace($dir)) { $dir = $default }
}

$hostArg = @()
if ($args.Count -ge 2) {
  $hostArg = @('--host', $args[1])
} else {
  $h = Read-Host 'Controller host for live settings (blank to skip) [192.168.37.237]'
  if (-not [string]::IsNullOrWhiteSpace($h)) {
    if ($h.Trim() -eq '') { }
    else { $hostArg = @('--host', $h.Trim()) }
  } else {
    $hostArg = @('--host', '192.168.37.237')
  }
}

& $env:COOLROOM_LOG_TOOLS_PYTHON (Join-Path $env:COOLROOM_TOOLS_DIR 'analyze_coolroom_logs.py') $dir @hostArg
