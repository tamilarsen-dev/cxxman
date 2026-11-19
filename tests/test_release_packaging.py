"""Tests for platform release archive assembly."""

import tarfile
import zipfile

from tools.package_release import create_archive


def _sample_files(tmp_path, names: tuple[str, ...]):
    paths = []
    for name in names:
        path = tmp_path / name
        path.write_text(f"sample {name}", encoding="utf-8")
        paths.append(path)
    return paths


def test_windows_release_is_a_zip_with_expected_members(tmp_path) -> None:
    paths = _sample_files(
        tmp_path,
        ("cxxman.exe", "install.ps1", "uninstall.ps1", "README.md"),
    )

    archive_path = create_archive("windows-x64", paths, tmp_path)

    with zipfile.ZipFile(archive_path) as archive:
        assert set(archive.namelist()) == {path.name for path in paths}


def test_posix_release_is_a_tarball_with_executable_modes(tmp_path) -> None:
    paths = _sample_files(
        tmp_path,
        ("cxxman", "install.sh", "uninstall.sh", "README.md"),
    )

    archive_path = create_archive("linux-x64", paths, tmp_path)

    with tarfile.open(archive_path, "r:gz") as archive:
        members = {member.name: member for member in archive.getmembers()}
    assert set(members) == {path.name for path in paths}
    assert members["cxxman"].mode == 0o755
    assert members["install.sh"].mode == 0o755
    assert members["uninstall.sh"].mode == 0o755
