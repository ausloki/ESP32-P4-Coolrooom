#!/usr/bin/env python3
"""Configure versioned repo hooks at .githooks for this repository."""

from __future__ import annotations

import os
import stat
import subprocess
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HOOKS = ROOT / ".githooks"

if os.name != "nt":
    os.environ["PATH"] = f"/usr/bin:/bin:/usr/sbin:/sbin:{os.environ.get('PATH', '')}"


def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(ROOT), text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def main() -> int:
    if not (ROOT / ".git").exists():
        print("ERROR: This script must run inside the repository root", file=sys.stderr)
        return 2

    HOOKS.mkdir(parents=True, exist_ok=True)

    git_exe = shutil.which("git") or ("/usr/bin/git" if os.name != "nt" and Path("/usr/bin/git").exists() else None)
    if not git_exe:
        print("ERROR: git executable not found in PATH", file=sys.stderr)
        return 2

    cp = run([git_exe, "config", "--local", "core.hooksPath", ".githooks"])
    if cp.returncode != 0:
        print(cp.stderr.strip() or "Failed to set core.hooksPath", file=sys.stderr)
        return 2

    if os.name != "nt":
        for hook in HOOKS.iterdir():
            if hook.is_file():
                mode = hook.stat().st_mode
                hook.chmod(mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    print("Configured core.hooksPath=.githooks")
    print("Hooks will run on checkout/merge/pre-commit and validate local dependencies.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
