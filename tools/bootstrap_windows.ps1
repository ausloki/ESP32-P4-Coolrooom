#requires -Version 5.1
<#!
.SYNOPSIS
  Windows bootstrap for ESP32-P4 Coolroom developer environment.

.DESCRIPTION
  - Locates Python launcher (py) or python executable
  - Creates .venv when missing
  - Installs dependencies from requirements.txt via dependency_check.py --install
  - Configures Git hooksPath to .githooks
#!>

$ErrorActionPreference = 'Stop'

function Get-PythonCommand {
  if (Get-Command py -ErrorAction SilentlyContinue) {
    return @('py', '-3')
  }
  if (Get-Command python -ErrorAction SilentlyContinue) {
    return @('python')
  }
  throw 'Python 3 was not found. Install Python 3.11+ and retry.'
}

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$PythonCmd = Get-PythonCommand
$VenvPython = Join-Path $RepoRoot '.venv\Scripts\python.exe'

if (-not (Test-Path $VenvPython)) {
  Write-Host '[bootstrap] Creating .venv...'
  & $PythonCmd[0] @($PythonCmd[1..($PythonCmd.Length-1)]) -m venv .venv
}

if (-not (Test-Path $VenvPython)) {
  throw 'Failed to create .venv (python executable not found inside .venv).'
}

Write-Host '[bootstrap] Installing dependencies using dependency_check.py --install...'
& $VenvPython tools/dependency_check.py --install

Write-Host '[bootstrap] Configuring Git hooks...'
& $VenvPython tools/setup_git_hooks.py

Write-Host '[bootstrap] Complete. Quick verification:'
& $VenvPython tools/dependency_check.py --quick
