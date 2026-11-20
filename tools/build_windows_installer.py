"""Compile the per-user Windows installer with Inno Setup."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def locate_compiler() -> Path:
    """Find the Inno Setup command-line compiler on Windows."""
    compiler = shutil.which("ISCC.exe") or shutil.which("iscc")
    if compiler:
        return Path(compiler)
    program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    candidate = Path(program_files_x86) / "Inno Setup 6" / "ISCC.exe"
    if candidate.is_file():
        return candidate
    raise SystemExit("Inno Setup 6 (ISCC.exe) is required to build the Windows installer")


def main() -> int:
    if sys.platform != "win32":
        raise SystemExit("The Windows installer must be built on Windows")
    executable = ROOT / "dist" / "cxxman.exe"
    if not executable.is_file():
        raise SystemExit(f"Standalone application executable not found: {executable}")
    compiler = locate_compiler()
    script = ROOT / "installer" / "cxxman.iss"
    version = _read_version()
    subprocess.run(
        [str(compiler), f"/DAppVersion={version}", str(script)],
        cwd=ROOT,
        check=True,
    )
    output = ROOT / "dist" / "cxxman-windows-x64-setup.exe"
    if not output.is_file():
        raise SystemExit(f"Installer compiler did not create {output}")
    print(f"Created {output}")
    return 0


def _read_version() -> str:
    sys.path.insert(0, str(ROOT / "src"))
    from cxxman import __version__

    return __version__


if __name__ == "__main__":
    raise SystemExit(main())
