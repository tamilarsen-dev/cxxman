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
  core/document.py        typed document and content-block model
  core/search.py          search result and service types
  core/update.py          catalog, downloads, parsing, and update flow
  parser/html_parser.py   source HTML to structured documents
  storage/database.py     SQLite schema, persistence, lookup, and search
  ui/renderer.py          Rich terminal rendering and pagination
tests/                    offline pytest suite and saved HTML fixtures
tools/benchmark.py        repeatable synthetic-corpus benchmark
docs/                     scope, architecture, decisions, and manuals
```

## Runtime boundaries

There are two separate paths:

```text
Read:    CLI -> SQLite -> Document -> Rich renderer -> terminal
Update:  CLI -> HTTPX -> Beautiful Soup parser -> SQLite transaction
```

Keep page display, search, and listing offline. Do not add network access
or HTML parsing to those commands. Updates fetch only topics in
`core/update.py`'s curated `SOURCE_PAGES` catalog and save parsed documents
through the database transaction. Parsing should remain independent from
terminal output and persistence.

The database stores document metadata, structured blocks as JSON, and
searchable text. It uses indexed SQLite lookup and FTS5 when the Python
SQLite build supports it. Without FTS5, a bounded fallback query is used.
Page lookup loads one page; search returns bounded metadata results.

## Adding a supported reference topic

The current download catalog is `SOURCE_PAGES` in
`src/cxxman/core/update.py`. To add a topic:

1. Add its topic name, stable identifier, cppreference URL, and header to
   the catalog.
2. Save a representative HTML fixture under `tests/fixtures/`.
3. Add parser coverage for any source structures not already represented.
4. Add update tests for URL selection, persistence, and failure behavior.
5. Mention the topic and command in `docs/user-manual.md`.

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

## Current release status

The project is currently an editable Python package for development. A
standalone Windows executable distributed through GitHub Releases is a
planned next stage; there is not yet a release build workflow or user
installer. See `docs/roadmap.md` for milestone status.
