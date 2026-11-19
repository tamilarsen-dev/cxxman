"""Assemble a platform-specific executable archive and SHA-256 sidecar."""

from __future__ import annotations

import argparse
import hashlib
import tarfile
import zipfile
from pathlib import Path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, choices=(
        "windows-x64", "linux-x64", "macos-x64", "macos-arm64"
    ))
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--installer", required=True, type=Path)
    parser.add_argument("--uninstaller", required=True, type=Path)
    parser.add_argument("--readme", required=True, type=Path)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    paths = [args.executable, args.installer, args.uninstaller, args.readme]
    missing = [str(path) for path in paths if not path.is_file()]
    if missing:
        raise SystemExit(f"Release inputs not found: {', '.join(missing)}")

    archive_path = create_archive(args.platform, paths, args.executable.resolve().parent)
    digest = hashlib.sha256(archive_path.read_bytes()).hexdigest()
    checksum_path = archive_path.with_name(archive_path.name + ".sha256")
    checksum_path.write_text(f"{digest}  {archive_path.name}\n", encoding="ascii")
    print(f"Created {archive_path}")
    print(f"SHA-256 {digest}")
    return 0


def create_archive(platform: str, paths: list[Path], output_directory: Path) -> Path:
    suffix = ".zip" if platform == "windows-x64" else ".tar.gz"
    archive_path = output_directory / f"cxxman-{platform}{suffix}"
    if suffix == ".zip":
        with zipfile.ZipFile(
            archive_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
        ) as archive:
            for path in paths:
                archive.write(path, arcname=path.name)
    else:
        with tarfile.open(archive_path, "w:gz") as archive:
            for path in paths:
                info = archive.gettarinfo(str(path), arcname=path.name)
                if path == paths[0] or path.suffix == ".sh":
                    info.mode = 0o755
                with path.open("rb") as source:
                    archive.addfile(info, source)
    return archive_path


if __name__ == "__main__":
    raise SystemExit(main())
