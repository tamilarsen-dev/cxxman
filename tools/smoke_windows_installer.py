"""Install and uninstall the built Windows setup in an isolated directory."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile


def main() -> int:
    if os.name != "nt":
        raise SystemExit("The Windows installer smoke check must run on Windows")
    root = Path(__file__).resolve().parents[1]
    installer = root / "dist" / "cxxman-windows-x64-setup.exe"
    if not installer.is_file():
        raise SystemExit(f"Windows installer not found: {installer}")

    with tempfile.TemporaryDirectory(prefix="cxxman-installer-smoke-") as directory:
        install_dir = Path(directory) / "application"
        subprocess.run(
            [
                str(installer),
                "/VERYSILENT",
                "/SUPPRESSMSGBOXES",
                "/NORESTART",
                "/CURRENTUSER",
                f"/DIR={install_dir}",
            ],
            check=True,
            timeout=120,
        )
        executable = install_dir / "cxxman.exe"
        uninstaller = install_dir / "unins000.exe"
        if not executable.is_file() or not uninstaller.is_file():
            raise SystemExit("Setup did not install cxxman and its uninstaller")
        subprocess.run(
            [
                str(uninstaller),
                "/VERYSILENT",
                "/SUPPRESSMSGBOXES",
                "/NORESTART",
            ],
            check=True,
            timeout=120,
        )
        if install_dir.exists():
            raise SystemExit("The generated uninstaller did not remove the app")
    print("Windows setup and generated uninstaller passed the smoke check.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
