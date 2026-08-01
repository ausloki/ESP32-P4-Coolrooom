# Shared env bootstrap for coolroom log tools (Windows PowerShell).
# Creates tools\.venv-log-tuning and installs requirements_log_tuning.txt.
# Sets $env:COOLROOM_LOG_TOOLS_PYTHON for the caller.

$ErrorActionPreference = 'Stop'

$ToolsDir = $PSScriptRoot
$RepoRoot = Split-Path -Parent $ToolsDir
$VenvDir = Join-Path $ToolsDir '.venv-log-tuning'
$Req = Join-Path $ToolsDir 'requirements_log_tuning.txt'
$Marker = Join-Path $VenvDir '.deps_ok'
$VenvPython = Join-Path $VenvDir 'Scripts\python.exe'

function Get-SystemPython {
  if (Get-Command py -ErrorAction SilentlyContinue) {
    return @{ Exe = 'py'; Args = @('-3') }
  }
  if (Get-Command python -ErrorAction SilentlyContinue) {
    return @{ Exe = 'python'; Args = @() }
  }
  throw 'Python 3 not found. Install Python 3.10+ from https://www.python.org/downloads/ and tick "Add python.exe to PATH".'
}

if (-not (Test-Path $VenvPython)) {
  Write-Host "[log-tools] Creating venv at $VenvDir ..."
  $sys = Get-SystemPython
  & $sys.Exe @($sys.Args + @('-m', 'venv', $VenvDir))
}

if (-not (Test-Path $VenvPython)) {
  throw "Failed to create venv python at $VenvPython"
}

$needInstall = -not (Test-Path $Marker)
if (-not $needInstall) {
  $needInstall = (Get-Item $Req).LastWriteTime -gt (Get-Item $Marker).LastWriteTime
}

if ($needInstall) {
  Write-Host '[log-tools] Installing dependencies (stdlib tools — pip check only) ...'
  & $VenvPython -m pip install --upgrade pip setuptools wheel | Out-Null
  & $VenvPython -m pip install -r $Req
  Set-Content -Path $Marker -Value (Get-Date).ToString('o')
  Write-Host '[log-tools] Environment ready.'
}

$env:COOLROOM_LOG_TOOLS_PYTHON = $VenvPython
$env:COOLROOM_REPO_ROOT = $RepoRoot
$env:COOLROOM_TOOLS_DIR = $ToolsDir
