#!/usr/bin/env python3
"""Cross-platform project dependency checker for ESP32-P4 Coolroom.

Usage:
  python tools/dependency_check.py
  python tools/dependency_check.py --install
  python tools/dependency_check.py --quick
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import venv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENV_DIR = ROOT / ".venv"
REQUIRED_MODULES = ["esphome", "code_review_graph", "certifi"]
REQUIREMENTS_FILE = ROOT / "requirements.txt"

if os.name != "nt":
    os.environ["PATH"] = f"/usr/bin:/bin:/usr/sbin:/sbin:{os.environ.get('PATH', '')}"


def run(cmd: list[str], cwd: Path | None = None, check: bool = False) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=check,
    )


def find_host_python() -> str | None:
    candidates: list[list[str]] = []
    if os.name == "nt":
        candidates.append(["py", "-3"])
    candidates.extend([["python3"], ["python"]])

    for c in candidates:
        try:
            cp = run(c + ["-c", "import sys; print(sys.executable)"])
            if cp.returncode == 0:
                return c[0]
        except FileNotFoundError:
            pass
    return None


def venv_python() -> Path:
    if os.name == "nt":
        return VENV_DIR / "Scripts" / "python.exe"
    return VENV_DIR / "bin" / "python"


def ensure_venv(host_python: str) -> tuple[bool, list[str]]:
    notes: list[str] = []
    created = False

    if not VENV_DIR.exists():
        builder = venv.EnvBuilder(with_pip=True)
        builder.create(str(VENV_DIR))
        created = True
        notes.append(f"Created virtual environment at {VENV_DIR}")

    py = venv_python()
    if not py.exists():
        raise RuntimeError(f"Virtualenv python not found at {py}")

    cp = run([str(py), "-m", "pip", "install", "--upgrade", "pip"])
    if cp.returncode != 0:
        raise RuntimeError(cp.stderr.strip() or "pip upgrade failed")

    if REQUIREMENTS_FILE.exists():
        cp = run([str(py), "-m", "pip", "install", "-r", str(REQUIREMENTS_FILE)])
    else:
        cp = run([str(py), "-m", "pip", "install", *REQUIRED_MODULES])
    if cp.returncode != 0:
        raise RuntimeError(cp.stderr.strip() or "dependency installation failed")

    notes.append("Installed required Python modules in .venv")
    return created, notes


def module_missing(module: str) -> bool:
    py = venv_python()
    cp = run([str(py), "-c", f"import {module}"])
    return cp.returncode != 0


def check_hooks_path() -> tuple[bool, str]:
    git_exe = shutil.which("git") or ("/usr/bin/git" if os.name != "nt" and Path("/usr/bin/git").exists() else None)
    if not git_exe:
        return False, ""
    cp = run([git_exe, "config", "--local", "--get", "core.hooksPath"], cwd=ROOT)
    hooks_path = cp.stdout.strip() if cp.returncode == 0 else ""
    ok = hooks_path == ".githooks"
    return ok, hooks_path


def check_quick() -> tuple[bool, list[str], list[str]]:
    errors: list[str] = []
    notes: list[str] = []

    host_python = find_host_python()
    if not host_python:
        errors.append("Python 3 executable not found. Install Python 3.11+.")
        return False, errors, notes

    notes.append(f"Host Python command found: {host_python}")

    if REQUIREMENTS_FILE.exists():
        notes.append(f"Requirements manifest found: {REQUIREMENTS_FILE.name}")
    else:
        notes.append("No requirements.txt found; using built-in module list")

    py = venv_python()
    if not py.exists():
        errors.append("Virtual environment missing. Run: python tools/dependency_check.py --install")
        return False, errors, notes

    for module in REQUIRED_MODULES:
        if module_missing(module):
            errors.append(
                f"Missing module in .venv: {module}. Run: python tools/dependency_check.py --install"
            )

    if not shutil.which("git"):
        errors.append("git executable not found in PATH")

    hooks_ok, hooks_path = check_hooks_path()
    if not hooks_ok:
        current = hooks_path or "<unset>"
        errors.append(
            f"Git hooks path not configured for repo. Current: {current}. Run: python tools/setup_git_hooks.py"
        )
    else:
        notes.append("Git hooks path is configured to .githooks")

    return len(errors) == 0, errors, notes


def print_report(ok: bool, errors: list[str], notes: list[str]) -> None:
    print("Dependency check report")
    for n in notes:
        print(f" - OK: {n}")
    for e in errors:
        print(f" - ERROR: {e}")
    print("Status: PASS" if ok else "Status: FAIL")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--install", action="store_true", help="Create .venv and install required modules")
    parser.add_argument("--quick", action="store_true", help="Run quick checks only")
    args = parser.parse_args()

    if args.install:
        host_python = find_host_python()
        if not host_python:
            print("ERROR: No Python 3 executable found (py/python3/python)", file=sys.stderr)
            return 2
        try:
            _, notes = ensure_venv(host_python)
            for n in notes:
                print(n)
        except Exception as exc:  # pragma: no cover
            print(f"ERROR: {exc}", file=sys.stderr)
            return 2

    ok, errors, notes = check_quick()
    print_report(ok, errors, notes)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
