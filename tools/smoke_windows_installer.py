"""Install and uninstall the built Windows setup in an isolated directory."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import winreg


def main() -> int:
    if os.name != "nt":
        raise SystemExit("The Windows installer smoke check must run on Windows")
    root = Path(__file__).resolve().parents[1]
    installer = root / "dist" / "cxxman-windows-x64-setup.exe"
    if not installer.is_file():
        raise SystemExit(f"Windows installer not found: {installer}")

    with tempfile.TemporaryDirectory(
        prefix="cxxman-installer-smoke-", ignore_cleanup_errors=True
    ) as directory:
        install_dir = Path(directory) / "application"
        setup_log = Path(directory) / "setup.log"
        _run_installer(
            [
                str(installer),
                "/VERYSILENT",
                "/SUPPRESSMSGBOXES",
                "/NORESTART",
                "/CURRENTUSER",
                f"/DIR={install_dir}",
            ],
            setup_log,
        )
        executable = install_dir / "cxxman.exe"
        uninstaller = install_dir / "unins000.exe"
        if not executable.is_file() or not uninstaller.is_file():
            raise SystemExit("Setup did not install cxxman and its uninstaller")
        if not _user_path_contains(install_dir):
            raise SystemExit("Setup did not add the installation folder to the user PATH")
        uninstall_log = Path(directory) / "uninstall.log"
        _run_installer(
            [
                str(uninstaller),
                "/VERYSILENT",
                "/SUPPRESSMSGBOXES",
                "/NORESTART",
            ],
            uninstall_log,
        )
        if executable.exists():
            raise SystemExit("The generated uninstaller did not remove cxxman.exe")
        if _user_path_contains(install_dir):
            raise SystemExit("The generated uninstaller left the installation folder in PATH")
    print("Windows setup and generated uninstaller passed the smoke check.")
    return 0


def _run_installer(command: list[str], log_path: Path) -> None:
    try:
        subprocess.run(
            [*command, f"/LOG={log_path}"],
            check=True,
            timeout=120,
        )
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
        if log_path.is_file():
            print(log_path.read_text(encoding="utf-8", errors="replace"))
        raise


def _user_path_contains(directory: Path) -> bool:
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Environment") as key:
            path_value, _ = winreg.QueryValueEx(key, "Path")
    except FileNotFoundError:
        return False
    expected = os.path.normcase(os.path.normpath(str(directory))).rstrip("\\/")
    entries = (
        os.path.normcase(os.path.normpath(entry.strip().strip('"'))).rstrip("\\/")
        for entry in path_value.split(";")
        if entry.strip()
    )
    return expected in entries


if __name__ == "__main__":
    raise SystemExit(main())
