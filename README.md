# cxxman

**C++ reference pages in your terminal, with offline search and on-demand updates.**

`cxxman` turns supported cppreference pages into concise, source-backed
terminal manual pages. It keeps downloaded pages in a local SQLite database:
page viewing, search, and topic discovery work offline.

## What you can do

- Read quick cards with the purpose, header, signatures, parameters, return
  behavior, complexity, important pitfalls, and a source example when those
  facts are available.
- Use `--full` to inspect the preserved parsed page and `--example` to show
  examples. The quick card does not replace the detailed source content.
- Search cached documentation, browse topics by header, and update one page or
  the full supported catalog.
- Keep normal reads offline. Only explicit documentation updates and
  application update commands use the network.
- Install a standalone build on Windows x86 or x64, Linux x64, or Intel and
  Apple Silicon macOS.

## Current release

Release **v0.3.1** adds a native Windows x86 build alongside Windows x64,
Linux x64, Intel macOS, and Apple Silicon macOS. Download the installer or
archive for your platform from
[GitHub Releases](https://github.com/tamilarsen-dev/cxxman/releases/latest).
The release includes SHA-256 checksums; the Windows setup executables also
have individual checksum files.

| Platform | Recommended download |
|---|---|
| 64-bit Windows | `cxxman-windows-x64-setup.exe` |
| 32-bit Windows | `cxxman-windows-x86-setup.exe` |
| Linux x64 | `cxxman-linux-x64.tar.gz` |
| Intel macOS | `cxxman-macos-x64.tar.gz` |
| Apple Silicon macOS | `cxxman-macos-arm64.tar.gz` |

Windows installers are per-user and do not require administrator access.
For Linux and macOS, extract the matching archive and run `sh ./install.sh`.
See the [installation and update guide](docs/release.md) for details,
checksums, and uninstall steps. macOS builds are not signed or notarized.

## Catalog and scope

The release includes a generated catalog of **2,496 names across 111
headers**, mapped to **2,167 cppreference routes** in the supported `/cpp/`
scope. This is a curated C++ standard-library reference catalog, not all of
cppreference. It excludes C-language pages, C++ language-reference pages,
experimental and TS pages, compiler-support tables, and library overview
pages. Catalog coverage does not mean every technical fact has been manually
audited.

The executable includes the catalog, but not downloaded page contents. Run an
explicit update to fetch pages into the local database. Cached page reads,
search, listing, and header discovery stay offline.

## Quick start

```console
cxxman topics
cxxman update vector::push_back
cxxman vector::push_back
cxxman search vector
cxxman header vector
```

To download all supported pages, run `cxxman update`. If an update stops,
continue with `cxxman update --resume`. Use `cxxman --help` to see all
commands and options.

## Reading pages

```console
cxxman vector
cxxman sort --example
cxxman vector --full
cxxman vector --color never
```

Quick cards select information supported by the stored source. They do not
invent missing signatures, guarantees, or standard versions. `--full` keeps
the complete parsed document available, and source links provide attribution
to cppreference. Rich rendering supports C++ syntax highlighting and
`--color auto|always|never`.

## Updating the application

```console
cxxman check-updates
cxxman self-update
```

These commands check GitHub for a newer standalone release and verify its
checksums before installing it. Python-managed installations should update
with pip or their source checkout. They are separate from `cxxman update`,
which downloads C++ reference pages.

## Run from source

The project uses Python 3.15.2 for development. From the repository root in
PowerShell:

```powershell
py -3.15 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -e ".[dev]"
python -m cxxman --help
```

## Documentation

- [User manual](docs/user-manual.md): commands, reading pages, search, and
  local data.
- [Release and installation](docs/release.md): platform assets, checksums,
  upgrades, and uninstall steps.
- [Architecture](docs/architecture.md): parser, storage, search, and rendering
  boundaries.
- [Developer guide](docs/developer-guide.md): local development and project
  workflows.
- [Roadmap](docs/roadmap.md): completed milestones and next work.
- [Project history and feature reference](docs/project-history-and-features.md):
  project evolution and current capabilities.
- [Coverage report](docs/coverage-report.md): catalog route and structural
  validation results.

## Project status

The latest release is **v0.3.1**. Its Windows x86 and x64 installers,
Linux x64 build, Intel macOS build, and Apple Silicon build passed the
GitHub Actions release workflow. The Windows CI job also tests installation
and removal of both installers. The local test suite passes **108 tests**.

The project is licensed under the [MIT License](LICENSE). See the
[release readiness notes](docs/release-readiness.md) for release history and
platform limitations.
