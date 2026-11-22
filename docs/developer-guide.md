# cxxman Developer Guide

This guide describes the current source tree, development workflow, and
the boundaries future changes should preserve.

## Requirements

- Windows development is currently tested with PowerShell and Python
  3.15.2; package metadata allows Python 3.12 and newer.
- Runtime dependencies: Beautiful Soup 4, HTTPX, and Rich.
- Development/test dependency: pytest.

The project uses `argparse`, `sqlite3`, and dataclasses from the Python
standard library. Do not install dependencies globally.

## Set up the development environment

From the repository root in PowerShell:

```powershell
py -3.15 -m venv .venv
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned
.\.venv\Scripts\Activate.ps1
python -m pip install -e .
python -m pip install pytest
```

Run the app and tests with the same interpreter:

```powershell
python -m cxxman --help
python -m pytest
```

The `cxxman` console command is installed by the editable package. The
project `.venv` and generated documentation databases are ignored by Git.

## Repository layout

```text
src/cxxman/
  __main__.py             python -m entry point
  cli.py                  argument parsing and command dispatch
  core/catalog.py         curated downloadable page metadata
  core/document.py        typed document and content-block model
  core/manual.py          conservative semantic projection to manual pages
  core/search.py          search result and service types
  core/update.py          downloads, parsing, and transactional update flow
  parser/html_parser.py   source HTML to structured documents
  storage/database.py     SQLite schema, persistence, lookup, and search
  ui/renderer.py          Rich terminal rendering and pagination
tests/                    offline pytest suite and saved HTML fixtures
scripts/                  POSIX per-user installers and uninstallers
installer/cxxman.iss      Windows per-user setup and generated uninstaller
tools/                     benchmark, packaging, and executable smoke checks
.github/workflows/         native platform build and release workflow
docs/                     scope, architecture, decisions, and manuals
```

## Runtime boundaries

There are two separate paths:

```text
Read:    CLI -> SQLite -> Document -> manual projection -> Rich renderer -> terminal
Update:  CLI -> HTTPX -> Beautiful Soup parser -> SQLite transaction
```

Keep page display, search, and listing offline. Do not add network access
or HTML parsing to those commands. Updates fetch only topics in the generated
`SOURCE_PAGES` catalog and save parsed documents through the database
transaction. Parsing should remain independent from
terminal output and persistence. Manual projection consumes the generic
`Document`, stays deterministic and offline, and leaves the stored source
document intact. Rich renders the prepared manual page; it does not classify
cppreference HTML.

The database stores document metadata, structured blocks as JSON, and
searchable text. It uses indexed SQLite lookup and FTS5 when the Python
SQLite build supports it. Without FTS5, a bounded fallback query is used.
Page lookup loads one page; search returns bounded metadata results.

## Adding a supported reference topic

The generated catalog is loaded lazily as `SOURCE_PAGES` by
`src/cxxman/core/catalog.py`; its checked-in data is
`src/cxxman/data/catalog.json`, built by `tools/build_catalog.py`. Each
entry includes its topic, stable source identifier and URL, header, entity
kind, and available aliases/purpose metadata. The original 50 topics remain
marked as the historical default-update set, while a no-topic update in the
current source tree fetches the full generated catalog. To add a topic:

1. Update the catalog source/builder data and regenerate the catalog. Keep
   topic, identifier, and URL values unique.
2. Save a representative HTML fixture under `tests/fixtures/`.
3. Add parser coverage for any source structures not already represented.
4. Add update tests for URL selection, persistence, and failure behavior.
5. Mention the topic and command in `docs/user-manual.md`.

Catalog shape and resolution are checked in `tests/test_catalog.py` and
`tests/test_catalog_parser.py`. Some cppreference pages cover multiple
related functions; use one source page entry for a shared page instead of
duplicating its URL. Review catalog scope and attribution rules in
`docs/quick-reference-spec.md` before changing the builder.

The parser should preserve semantic content blocks and degrade gracefully
when page markup changes. Never use live network access in ordinary unit
tests; inject a fake `httpx.Client` or mocked response as the update tests
do.

## Test practices

Run the full suite before completing a milestone:

```powershell
python -m pytest
```

Tests are organized by boundary: CLI, document model, parser, renderer,
search, storage, and update. Use `tmp_path` for SQLite files and saved
HTML fixtures for parser tests. The read-path tests assert that HTTP is
not called. Update tests simulate network responses and failures.

## Performance work

Use `python tools/benchmark.py` for the project benchmark. It records
synthetic corpus measurements; do not present them as real cppreference
corpus measurements. See `docs/performance.md` for the environment,
method, and recorded baseline. Profile before adding caches or changing
storage behavior.

## Design and contribution rules

- Keep one coherent milestone per change and avoid unrelated refactors.
- Check `git status` before editing and inspect `git diff` before a
  milestone is committed.
- Preserve the local-first read path and explicit update boundary.
- Use typed document blocks; do not flatten whole pages into one string.
- Keep HTTP, parsing, database queries, and rendering in their assigned
  modules.
- Add deterministic tests for behavior changes; do not require internet
  access for the default test suite.
- Keep generated databases, downloaded data, build output, and virtual
  environments out of commits.

## Building and checking a native executable

Install build tools into the project environment and build the one-file
console executable:

```powershell
python -m pip install -e ".[build,dev]"
python -m PyInstaller --noconfirm --clean --onefile --console `
  --name cxxman --paths src --specpath build/pyinstaller `
  --collect-data certifi --collect-submodules pygments.lexers `
  src/cxxman/__main__.py
python tools/smoke_executable.py dist/cxxman.exe
```

On Linux and macOS, the native executable is `dist/cxxman` rather than
`dist/cxxman.exe`. Run the build on the operating system and architecture
you want to target; PyInstaller does not make a Windows executable run
on another operating system. On Windows, install Inno Setup 6 and run
`python tools/build_windows_installer.py` after building the executable.
It creates the per-user setup `.exe` and generated Windows uninstaller.
`tools/package_release.py` assembles the native executable with the
platform installer, README, and SHA-256 checksums. The explicit app
updater lives in `core/app_updates.py`; page viewing and search do not use
it.

The smoke check uses a temporary SQLite database and does not access the
network. The GitHub Actions workflow repeats tests, build, packaging, and
smoke checks on Windows x64, Linux x64, Intel macOS, and Apple Silicon
macOS for pushes and pull requests. Windows also compiles, installs, and
uninstalls the Inno Setup package in a temporary directory. A matching
`v*` tag publishes platform assets, including the Windows setup `.exe`
and checksums. Read `docs/release.md` before tagging a release.
