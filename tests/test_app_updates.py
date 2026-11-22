"""Tests for explicit release checks and verified application updates."""

import hashlib
import io
import os
import tarfile

import httpx
import pytest

from cxxman.core.app_updates import (
    AppUpdateError,
    LatestRelease,
    ReleaseAsset,
    detect_platform,
    fetch_latest_release,
    install_release_update,
    is_update_available,
)


def _release_payload(assets: list[dict[str, str]]) -> dict[str, object]:
    return {
        "tag_name": "v1.2.0",
        "html_url": "https://github.com/tamilarsen-dev/cxxman/releases/tag/v1.2.0",
        "assets": assets,
    }


def test_fetch_latest_release_validates_github_release_payload() -> None:
    request = httpx.Request("GET", "https://api.github.test/releases/latest")
    response = httpx.Response(
        200,
        json=_release_payload(
            [
                {
                    "name": "cxxman-linux-x64.tar.gz",
                    "browser_download_url": (
                        "https://github.com/tamilarsen-dev/cxxman/releases/download/"
                        "v1.2.0/cxxman-linux-x64.tar.gz"
                    ),
                }
            ]
        ),
        request=request,
    )
    client = httpx.Client(transport=httpx.MockTransport(lambda _: response))

    release = fetch_latest_release(client=client)

    assert release.tag == "v1.2.0"
    assert release.version == "1.2.0"
    assert "cxxman-linux-x64.tar.gz" in release.assets


def test_fetch_latest_release_rejects_an_untrusted_release_url() -> None:
    payload = _release_payload([])
    payload["html_url"] = "https://attacker.example/releases/tag/v1.2.0"
    client = httpx.Client(
        transport=httpx.MockTransport(
            lambda request: httpx.Response(200, json=payload, request=request)
        )
    )

    with pytest.raises(AppUpdateError, match="missing its version or page URL"):
        fetch_latest_release(client=client)


@pytest.mark.parametrize(
    ("system", "machine", "expected"),
    [
        ("Windows", "AMD64", "windows-x64"),
        ("Windows", "x86", "windows-x86"),
        ("Linux", "x86_64", "linux-x64"),
        ("Darwin", "arm64", "macos-arm64"),
    ],
)
def test_detect_platform(system: str, machine: str, expected: str) -> None:
    assert detect_platform(system, machine, pointer_bits=64) == expected


def test_detect_windows_32_bit_process_on_64_bit_host() -> None:
    assert detect_platform("Windows", "AMD64", pointer_bits=32) == "windows-x86"


def test_version_comparison_uses_numeric_components() -> None:
    assert is_update_available("0.9.0", "0.10.0")
    assert not is_update_available("1.2.0", "1.2.0")
    assert not is_update_available("1.3.0", "1.2.9")


def _tarball(contents: bytes) -> bytes:
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w:gz") as archive:
        info = tarfile.TarInfo("cxxman")
        info.size = len(contents)
        info.mode = 0o755
        archive.addfile(info, io.BytesIO(contents))
    return buffer.getvalue()


def _mocked_assets(asset_name: str, asset_bytes: bytes, checksum: str) -> httpx.Client:
    urls: dict[str, tuple[bytes, str]] = {
        f"https://release.test/{asset_name}": (asset_bytes, "application/octet-stream"),
        f"https://release.test/{asset_name}.sha256": (
            f"{checksum}  {asset_name}\n".encode("ascii"), "text/plain"
        ),
    }

    def handler(request: httpx.Request) -> httpx.Response:
        content, content_type = urls[str(request.url)]
        return httpx.Response(
            200,
            content=content,
            headers={"Content-Type": content_type},
            request=request,
        )

    return httpx.Client(transport=httpx.MockTransport(handler))


def test_linux_update_verifies_archive_and_replaces_executable(tmp_path) -> None:
    executable = tmp_path / "cxxman"
    executable.write_bytes(b"old executable")
    executable.chmod(0o755)
    archive = _tarball(b"new executable")
    name = "cxxman-linux-x64.tar.gz"
    client = _mocked_assets(name, archive, hashlib.sha256(archive).hexdigest())
    release = LatestRelease(
        "v1.2.0",
        "1.2.0",
        "https://github.com/tamilarsen-dev/cxxman/releases/tag/v1.2.0",
        {
            name: ReleaseAsset(name, f"https://release.test/{name}"),
            f"{name}.sha256": ReleaseAsset(
                f"{name}.sha256", f"https://release.test/{name}.sha256"
            ),
        },
    )

    message = install_release_update(
        release,
        executable_path=executable,
        frozen=True,
        system="Linux",
        machine="x86_64",
        client=client,
    )

    assert executable.read_bytes() == b"new executable"
    if os.name == "posix":
        assert executable.stat().st_mode & 0o777 == 0o755
    assert "Updated cxxman to v1.2.0" in message


@pytest.mark.parametrize(
    ("machine", "platform_name", "pointer_bits"),
    [("AMD64", "windows-x64", 64), ("x86", "windows-x86", 32)],
)
def test_windows_update_launches_checksum_verified_setup(
    tmp_path, machine: str, platform_name: str, pointer_bits: int
) -> None:
    executable = tmp_path / "cxxman.exe"
    executable.write_bytes(b"current")
    installer = b"installer bytes"
    name = f"cxxman-{platform_name}-setup.exe"
    client = _mocked_assets(name, installer, hashlib.sha256(installer).hexdigest())
    release = LatestRelease(
        "v1.2.0",
        "1.2.0",
        "https://github.com/tamilarsen-dev/cxxman/releases/tag/v1.2.0",
        {
            name: ReleaseAsset(name, f"https://release.test/{name}"),
            f"{name}.sha256": ReleaseAsset(
                f"{name}.sha256", f"https://release.test/{name}.sha256"
            ),
        },
    )
    launched: list[list[str]] = []

    message = install_release_update(
        release,
        executable_path=executable,
        frozen=True,
        system="Windows",
        machine=machine,
        pointer_bits=pointer_bits,
        client=client,
        launch=launched.append,
    )

    assert len(launched) == 1
    assert launched[0][1:] == ["/CURRENTUSER", "/CLOSEAPPLICATIONS", "/NORESTARTAPPLICATIONS"]
    from pathlib import Path

    assert Path(launched[0][0]).read_bytes() == installer
    assert "Follow its prompts" in message


def test_update_rejects_bad_checksum_and_python_managed_installation(tmp_path) -> None:
    executable = tmp_path / "cxxman"
    executable.write_bytes(b"current")
    name = "cxxman-linux-x64.tar.gz"
    archive = _tarball(b"updated")
    client = _mocked_assets(name, archive, "0" * 64)
    release = LatestRelease(
        "v1.2.0",
        "1.2.0",
        "https://github.com/tamilarsen-dev/cxxman/releases/tag/v1.2.0",
        {
            name: ReleaseAsset(name, f"https://release.test/{name}"),
            f"{name}.sha256": ReleaseAsset(
                f"{name}.sha256", f"https://release.test/{name}.sha256"
            ),
        },
    )

    with pytest.raises(AppUpdateError, match="checksum verification failed"):
        install_release_update(
            release,
            executable_path=executable,
            frozen=True,
            system="Linux",
            machine="x86_64",
            client=client,
        )
    assert executable.read_bytes() == b"current"

    with pytest.raises(AppUpdateError, match="managed by Python"):
        install_release_update(release, executable_path=executable, frozen=False)
