# Release and Installation

## Supported release builds

GitHub Release `v0.1.0` includes standalone command-line builds. Users do
not need Python, pip, a virtual environment, or administrator privileges.

| Asset | Target |
|---|---|
| `cxxman-windows-x64.zip` | 64-bit Windows |
| `cxxman-linux-x64.tar.gz` | 64-bit Linux, built on Ubuntu 22.04 |
| `cxxman-macos-x64.tar.gz` | Intel macOS |
| `cxxman-macos-arm64.tar.gz` | Apple Silicon macOS |

PyInstaller builds for the native operating system and architecture; the
release workflow therefore uses one GitHub runner per target. The
executable contains the application, Python runtime, and Python package
dependencies. It does not include the C++ reference corpus. Pages are
downloaded explicitly and stored in the user's local database; page
viewing, search, and listing work offline.

## Windows installation

1. Download `cxxman-windows-x64.zip` from GitHub Releases.
2. Extract the ZIP to a folder.
3. Open PowerShell in that folder and run:

   ```powershell
   .\install.ps1
   ```

   If PowerShell blocks the downloaded script, use a process-only
   execution policy override:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1
   ```

4. Open a new terminal, then run `cxxman --help`.

The installer copies the executable to
`%LOCALAPPDATA%\Programs\cxxman` and adds that directory to the current
user's `PATH`.

## Linux and macOS installation

1. Download the archive matching your OS and CPU from GitHub Releases.
2. Extract it, then open a terminal in the extracted folder.
3. Run the installer:

   ```sh
   sh ./install.sh
   ```

4. Open a new terminal, then run `cxxman --help`.

The installer copies the executable and uninstaller to `~/.local/bin` and
adds that directory to the startup file for the current shell: `~/.bashrc`
for Bash, `~/.zshrc` for Zsh, or `~/.config/fish/config.fish` for Fish.
Other shells use `~/.profile`. If your shell does not read that file, add
`~/.local/bin` to its `PATH` manually.

macOS downloads are not currently signed or notarized. macOS may block
the first run; review the Gatekeeper message and allow the executable in
System Settings only if you trust the release and have verified its
checksum.

## Verify downloads

Each archive has a matching `.sha256` file. On Windows, compare:

```powershell
Get-FileHash .\cxxman-windows-x64.zip -Algorithm SHA256
Get-Content .\cxxman-windows-x64.zip.sha256
```

On Linux or macOS, compare:

```sh
shasum -a 256 cxxman-linux-x64.tar.gz
cat cxxman-linux-x64.tar.gz.sha256
```

Use the matching asset name for your download. The Windows executable
is not currently code-signed and may trigger a SmartScreen or antivirus
warning.

## Use

Inspect the supported pages, download one, then read it:

```sh
cxxman topics
cxxman update vector::push_back
cxxman vector::push_back
```

The current starter catalog contains `vector`, `vector::push_back`,
`string`, and `sort`. Run `cxxman --help` for the full command list.
Updates need an internet connection. Cached page reading, search, and
listing do not.

The local database is stored in the user's application data directory.
On Windows this is `%LOCALAPPDATA%\cxxman\cxxman.sqlite3`; on Linux and
macOS it is `$XDG_DATA_HOME/cxxman/cxxman.sqlite3`, or
`~/.local/share/cxxman/cxxman.sqlite3` when `XDG_DATA_HOME` is unset.
Upgrading or uninstalling the CLI leaves this database intact.

## Upgrade and uninstall

- **Windows:** extract a newer ZIP and run `install.ps1` again. Remove the
  app with `& "$env:LOCALAPPDATA\Programs\cxxman\uninstall.ps1"`.
- **Linux/macOS:** extract a newer archive and run `sh ./install.sh`
  again. Remove the app with `sh "$HOME/.local/bin/uninstall.sh"`.

Uninstall removes the executable, uninstaller, and PATH entry but keeps
downloaded pages. To delete the cache too, remove the `cxxman` folder in
the platform's data directory described above.

## Maintainer release steps

1. Set the same release version in `pyproject.toml` and
   `src/cxxman/__init__.py`.
2. Commit and push the version change.
3. Push a matching version tag, for example `v0.1.0`.
4. The workflow runs tests on four native runners, builds and smoke-tests
   each executable, packages each archive with its checksum, and creates
   one GitHub Release containing all assets.

The workflow rejects tags whose version does not match both project
version fields. Linux x64 releases target the Ubuntu 22.04 baseline.
Other Linux distributions may need a newer glibc than their system
provides. Windows ARM and Linux ARM releases are not included yet.
