"""Explicit GitHub release checks and verified application updates."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import io
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tarfile
import tempfile
from typing import Callable
import zipfile

import httpx

from cxxman import __version__

REPOSITORY = "tamilarsen-dev/cxxman"
LATEST_RELEASE_URL = f"https://api.github.com/repos/{REPOSITORY}/releases/latest"
_USER_AGENT = f"cxxman/{__version__} (application updater)"


@dataclass(frozen=True, slots=True)
class ReleaseAsset:
    """A downloadable file attached to a GitHub release."""

    name: str
    url: str


@dataclass(frozen=True, slots=True)
class LatestRelease:
    """Validated release data needed by the check and update commands."""

    tag: str
    version: str
    url: str
    assets: dict[str, ReleaseAsset]


class AppUpdateError(RuntimeError):
    """Expected API, download, checksum, or platform update failure."""


def fetch_latest_release(*, client: httpx.Client | None = None) -> LatestRelease:
    """Fetch and validate the latest stable release from GitHub."""
    if client is not None:
        return _fetch_latest_release(client)
    try:
        with httpx.Client(
            timeout=20.0,
            follow_redirects=True,
            headers={
                "Accept": "application/vnd.github+json",
                "X-GitHub-Api-Version": "2022-11-28",
                "User-Agent": _USER_AGENT,
            },
        ) as http_client:
            return _fetch_latest_release(http_client)
    except httpx.HTTPError as error:
        raise AppUpdateError(f"could not check GitHub releases: {error}") from error


def _fetch_latest_release(client: httpx.Client) -> LatestRelease:
    try:
        response = client.get(LATEST_RELEASE_URL)
        response.raise_for_status()
        payload = response.json()
    except httpx.HTTPError as error:
        raise AppUpdateError(f"could not check GitHub releases: {error}") from error
    except ValueError as error:
        raise AppUpdateError("GitHub returned invalid release data") from error

    if not isinstance(payload, dict):
        raise AppUpdateError("GitHub returned invalid release data")
    tag = payload.get("tag_name")
    page_url = payload.get("html_url")
    if (
        not isinstance(tag, str)
        or not isinstance(page_url, str)
        or not page_url.startswith(f"https://github.com/{REPOSITORY}/releases/")
    ):
        raise AppUpdateError("GitHub release is missing its version or page URL")
    version = _version_from_tag(tag)
    raw_assets = payload.get("assets")
    if not isinstance(raw_assets, list):
        raise AppUpdateError("GitHub release is missing its downloadable assets")
    assets: dict[str, ReleaseAsset] = {}
    for raw_asset in raw_assets:
        if not isinstance(raw_asset, dict):
            continue
        name = raw_asset.get("name")
        url = raw_asset.get("browser_download_url")
        expected_asset_prefix = f"https://github.com/{REPOSITORY}/releases/download/"
        if (
            isinstance(name, str)
            and isinstance(url, str)
            and url.startswith(expected_asset_prefix)
        ):
            assets[name] = ReleaseAsset(name, url)
    return LatestRelease(tag, version, page_url, assets)


def is_update_available(current_version: str, latest_version: str) -> bool:
    """Return whether a stable numeric version is newer than the current one."""
    return _version_key(latest_version) > _version_key(current_version)


def detect_platform(system: str | None = None, machine: str | None = None) -> str:
    """Map the current OS and CPU to a published release platform."""
    operating_system = (system or platform.system()).casefold()
    architecture = (machine or platform.machine()).casefold()
    if architecture in {"amd64", "x86_64", "x64"}:
        suffix = "x64"
    elif architecture in {"arm64", "aarch64"}:
        suffix = "arm64"
    else:
        raise AppUpdateError(f"application updates are not available for {architecture}")

    if operating_system == "windows":
        if suffix != "x64":
            raise AppUpdateError("application updates are not available for Windows ARM yet")
        return "windows-x64"
    if operating_system == "linux" and suffix == "x64":
        return "linux-x64"
    if operating_system == "darwin":
        return f"macos-{suffix}"
    raise AppUpdateError(f"application updates are not available for {operating_system} {suffix}")


def install_release_update(
    release: LatestRelease,
    *,
    executable_path: Path | None = None,
    frozen: bool | None = None,
    system: str | None = None,
    machine: str | None = None,
    client: httpx.Client | None = None,
    launch: Callable[[list[str]], object] = subprocess.Popen,
) -> str:
    """Download, verify, and apply or launch an update for this installation."""
    is_frozen = bool(getattr(sys, "frozen", False)) if frozen is None else frozen
    if not is_frozen:
        raise AppUpdateError(
            "this installation is managed by Python; update it with pip or reinstall from source"
        )
    platform_key = detect_platform(system, machine)
    target = (executable_path or Path(sys.argv[0])).resolve()
    if not target.is_file():
        raise AppUpdateError(f"could not locate the installed executable at {target}")

    if client is not None:
        return _install_release_update(release, platform_key, target, client, launch)
    try:
        with httpx.Client(
            timeout=60.0,
            follow_redirects=True,
            headers={"User-Agent": _USER_AGENT},
        ) as http_client:
            return _install_release_update(release, platform_key, target, http_client, launch)
    except httpx.HTTPError as error:
        raise AppUpdateError(f"could not download the application update: {error}") from error


def _install_release_update(
    release: LatestRelease,
    platform_key: str,
    target: Path,
    client: httpx.Client,
    launch: Callable[[list[str]], object],
) -> str:
    if platform_key == "windows-x64":
        setup_name = "cxxman-windows-x64-setup.exe"
        setup_bytes = _download_verified_asset(release, setup_name, client)
        file_descriptor, temporary_name = tempfile.mkstemp(
            prefix=f"cxxman-setup-{release.version}-", suffix=".exe"
        )
        staged_setup = Path(temporary_name)
        try:
            with os.fdopen(file_descriptor, "wb") as staged_file:
                staged_file.write(setup_bytes)
            launch(
                [
                    str(staged_setup),
                    "/CURRENTUSER",
                    "/CLOSEAPPLICATIONS",
                    "/NORESTARTAPPLICATIONS",
                ]
            )
        except OSError as error:
            staged_setup.unlink(missing_ok=True)
            raise AppUpdateError(f"could not start the Windows installer: {error}") from error
        return f"Started the {release.tag} installer. Follow its prompts to finish updating."

    archive_name = f"cxxman-{platform_key}.tar.gz"
    archive_bytes = _download_verified_asset(release, archive_name, client)
    executable_bytes = _read_posix_executable(archive_name, archive_bytes)
    _replace_executable(target, executable_bytes)
    return f"Updated cxxman to {release.tag}."


def _download_verified_asset(
    release: LatestRelease, asset_name: str, client: httpx.Client
) -> bytes:
    asset = release.assets.get(asset_name)
    checksum_asset = release.assets.get(f"{asset_name}.sha256")
    if asset is None or checksum_asset is None:
        raise AppUpdateError(f"release {release.tag} does not contain {asset_name} and its checksum")
    try:
        asset_response = client.get(asset.url)
        asset_response.raise_for_status()
        checksum_response = client.get(checksum_asset.url)
        checksum_response.raise_for_status()
    except httpx.HTTPError as error:
        raise AppUpdateError(f"could not download release asset {asset_name}: {error}") from error

    checksum_parts = checksum_response.text.strip().split(maxsplit=1)
    if not checksum_parts:
        raise AppUpdateError(f"release checksum for {asset_name} is invalid")
    expected = checksum_parts[0].casefold()
    if not re.fullmatch(r"[0-9a-f]{64}", expected):
        raise AppUpdateError(f"release checksum for {asset_name} is invalid")
    actual = hashlib.sha256(asset_response.content).hexdigest()
    if actual != expected:
        raise AppUpdateError(f"checksum verification failed for {asset_name}")
    return asset_response.content


def _read_posix_executable(archive_name: str, archive_bytes: bytes) -> bytes:
    try:
        if archive_name.endswith(".zip"):
            with zipfile.ZipFile(io.BytesIO(archive_bytes)) as archive:
                return archive.read("cxxman")
        with tarfile.open(fileobj=io.BytesIO(archive_bytes), mode="r:gz") as archive:
            member = archive.getmember("cxxman")
            if not member.isfile():
                raise AppUpdateError("release archive does not contain a regular cxxman executable")
            source = archive.extractfile(member)
            if source is None:
                raise AppUpdateError("could not read cxxman from the release archive")
            with source:
                return source.read()
    except (KeyError, tarfile.TarError, zipfile.BadZipFile, OSError) as error:
        raise AppUpdateError("release archive does not contain a readable cxxman executable") from error


def _replace_executable(target: Path, contents: bytes) -> None:
    temporary_path: Path | None = None
    try:
        file_descriptor, temporary_name = tempfile.mkstemp(
            prefix=f".{target.name}-update-", dir=target.parent
        )
        temporary_path = Path(temporary_name)
        with os.fdopen(file_descriptor, "wb") as staged_file:
            staged_file.write(contents)
        temporary_path.chmod(target.stat().st_mode)
        os.replace(temporary_path, target)
    except OSError as error:
        raise AppUpdateError(f"could not replace {target}: {error}") from error
    finally:
        if temporary_path is not None:
            try:
                temporary_path.unlink(missing_ok=True)
            except OSError:
                pass


def _version_from_tag(tag: str) -> str:
    version = tag.removeprefix("v")
    _version_key(version)
    return version


def _version_key(version: str) -> tuple[int, ...]:
    match = re.fullmatch(r"(\d+(?:\.\d+){1,3})(?:[-+][0-9A-Za-z.-]+)?", version)
    if match is None:
        raise AppUpdateError(f"unsupported release version '{version}'")
    parts = tuple(int(part) for part in match.group(1).split("."))
    return (*parts, *(0 for _ in range(4 - len(parts))))
