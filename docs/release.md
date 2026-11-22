# Release and Installation

## Supported release builds

Release `v0.3.1` provides standalone command-line builds. Users do not need
Python, pip, a virtual environment, PowerShell, or administrator
privileges to install the Windows package.

| Asset | Target |
|---|---|
| `cxxman-windows-x64.zip` | 64-bit Windows |
| `cxxman-windows-x64-setup.exe` | 64-bit Windows per-user installer |
| `cxxman-windows-x86.zip` | 32-bit Windows |
| `cxxman-windows-x86-setup.exe` | 32-bit Windows per-user installer |
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

Download and run the setup executable that matches Windows: use the x64
installer on 64-bit Windows or the x86 installer on 32-bit Windows. Open a
new terminal and run `cxxman --help`. Setup installs for the current user
under `%LOCALAPPDATA%\Programs\cxxman`, adds that folder to the user's
`PATH`, and creates a Start menu shortcut. It requires no PowerShell or
administrator access. The generated uninstaller is listed in Windows Apps
settings and is available as `unins000.exe` in the install folder.

The older `v0.1.0` release predates this installer and contains only the
standalone executable ZIP. Releases `v0.2.0` and later include the Windows
setup executable.

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

Each archive has a matching `.sha256` file. The Windows setup executable
also has its own checksum sidecar. On Windows, compare:

```bat
certutil -hashfile cxxman-windows-x64-setup.exe SHA256
type cxxman-windows-x64-setup.exe.sha256
```

For 32-bit Windows, compare `cxxman-windows-x86-setup.exe` with its
`cxxman-windows-x86-setup.exe.sha256` sidecar.

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

The release includes a generated catalog with 2,496 named entries across
111 headers, mapped to 2,167 cppreference routes in the supported `/cpp/`
scope. `cxxman update` downloads the catalog; `cxxman update <topic>` fetches
one page. Run `cxxman topics` to list topics and `cxxman --help` for the
full command list.

Manual pages use C++ syntax highlighting. `--color auto` follows terminal
capabilities by default; `--color always` and `--color never` override it.
`cxxman update` downloads reference pages. `cxxman check-updates` checks
GitHub for a newer application release, and `cxxman self-update` verifies
and installs it for standalone builds. These commands need an internet
connection. Cached page reading, search, and listing do not. Python-managed
installations should update with pip or reinstall from source.

The local database is stored in the user's application data directory.
On Windows this is `%LOCALAPPDATA%\cxxman\cxxman.sqlite3`; on Linux and
macOS it is `$XDG_DATA_HOME/cxxman/cxxman.sqlite3`, or
`~/.local/share/cxxman/cxxman.sqlite3` when `XDG_DATA_HOME` is unset.
Upgrading or uninstalling the CLI leaves this database intact.

## Upgrade and uninstall

- **Windows:** run a newer setup `.exe`; it closes cxxman during
  replacement and opens the setup wizard. Uninstall through Windows Apps
  settings or run `unins000.exe` from the installation folder. Downloaded
  pages remain installed.
- **Linux/macOS:** extract a newer archive and run `sh ./install.sh`
  again. Remove the app with `sh "$HOME/.local/bin/uninstall.sh"`.

Uninstall removes the executable, uninstaller, and PATH entry but keeps
downloaded pages. To delete the cache too, remove the `cxxman` folder in
the platform's data directory described above.

## Maintainer release steps

1. Set the same release version in `pyproject.toml` and
   `src/cxxman/__init__.py`.
2. Commit and push the version change.
3. Push a matching version tag, for example `v0.3.1`.
4. The workflow runs tests on five native build targets, builds and
   smoke-tests each executable, tests both Windows installers, packages
   archives with checksums, and creates one GitHub Release containing all
   assets.

The workflow rejects tags whose version does not match both project
version fields. Linux x64 releases target the Ubuntu 22.04 baseline.
Other Linux distributions may need a newer glibc than their system
provides. Windows ARM and Linux ARM releases are not included yet.
