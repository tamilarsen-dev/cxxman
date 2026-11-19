# Windows Release and Installation

## What the release contains

The Windows x64 release ZIP contains:

- `cxxman.exe`: the standalone terminal application, including its Python
  runtime and Python package dependencies.
- `install.ps1`: per-user installer that copies the executable and adds
  its folder to the user's `PATH`.
- `uninstall.ps1`: removes the executable and its `PATH` entry.
- This README with install and usage instructions.

Users do not need Python, pip, a virtual environment, or administrator
rights. The executable is built for 64-bit Windows. Other operating
systems and CPU architectures do not have release artifacts yet.

The executable does not include the C++ reference corpus. Pages are
downloaded on demand from cppreference and stored in the user's local
database. Ordinary page viewing, search, and listing remain offline.

## Install

1. Download `cxxman-windows-x64.zip` from the GitHub Releases page.
2. Extract the ZIP to a folder.
3. Open PowerShell in that extracted folder and run:

   ```powershell
   .\install.ps1
   ```

   If PowerShell blocks the downloaded script, run it with a process-only
   execution policy override:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1
   ```

4. Open a new terminal window so it loads the updated user `PATH`.
5. Check the installation and inspect the catalog:

   ```powershell
   cxxman --version
   cxxman topics
   ```

The installer writes the executable to
`%LOCALAPPDATA%\Programs\cxxman` and adds that directory to the current
Windows user's `PATH`. It does not need administrator privileges.

## Use

Download a page, then read it:

```powershell
cxxman update vector::push_back
cxxman vector::push_back
```

Or download the full current starter catalog:

```powershell
cxxman update
```

The current supported pages are `vector`, `vector::push_back`, `string`,
and `sort`. Run `cxxman topics` for the version's catalog and
`cxxman --help` for commands. Updates require an internet connection.
Cached page reading, search, and listing do not.

The local documentation database is kept under
`%LOCALAPPDATA%\cxxman\cxxman.sqlite3`. Updating the application does not
remove cached pages.

## Upgrade

Download and extract the newer release ZIP, then run its `install.ps1`
again. It replaces the application in the same per-user directory and
leaves the local documentation database intact. Open a new terminal if
the command is not found.

## Uninstall

Run the installed uninstaller:

```powershell
& "$env:LOCALAPPDATA\Programs\cxxman\uninstall.ps1"
```

The uninstaller removes the application directory and its user `PATH`
entry. It keeps the documentation database. To delete cached pages too,
remove `%LOCALAPPDATA%\cxxman` separately.

## Checksums and release trust

Each GitHub Release includes `cxxman-windows-x64.zip.sha256`. Verify the
download before extracting it:

```powershell
Get-FileHash .\cxxman-windows-x64.zip -Algorithm SHA256
Get-Content .\cxxman-windows-x64.zip.sha256
```

Compare the hash values. The executable is not currently code-signed, so
Windows SmartScreen or antivirus software may show a warning for a new
download.

## Maintainer release steps

1. Set the same release version in `pyproject.toml` and
   `src/cxxman/__init__.py`.
2. Commit the version change and push the commit to GitHub.
3. Create and push a matching tag, for example `v0.1.0`.
4. The Windows release workflow runs the tests, builds the one-file
   executable, smoke-tests the executable, assembles the ZIP and checksum,
   and publishes a GitHub Release.

The workflow rejects tags whose version does not match both project
version fields. The release is Windows x64 only; add and verify another
build job before advertising support for other platforms.
