# Technology Decisions

## Selected stack

  -----------------------------------------------------------------------
  Component               Choice                  Reason
  ----------------------- ----------------------- -----------------------
  Language                Python 3                Application logic and
                                                  ecosystem

  CLI                     `argparse`              Standard library;
                                                  lightweight startup

  HTML parser             BeautifulSoup4          Practical HTML parsing

HTTP client             HTTPX                   Explicit documentation
                                                  and application-release
                                                  updates

  Storage                 SQLite                  Local indexed storage;
                                                  built into Python

  Terminal rendering      Rich                    Tables, C++ syntax
                                                  highlighting, terminal
                                                  colors, and layout

  Testing                 Pytest                  Automated tests

  Profiling               `timeit`, `cProfile`    Standard-library
                                                  performance measurement
  -----------------------------------------------------------------------

## Dependency policy

-   Keep runtime dependencies minimal.
-   Do not add a dependency when the standard library is sufficient.
-   Install third-party packages inside the project virtual environment.
-   Pin or constrain dependencies in project configuration once
    compatibility is verified.
-   Test library compatibility with the project's selected Python
    version before relying on it.

## CLI decision: argparse

Use `argparse` rather than Typer for the initial version. It is built
into Python and avoids another runtime dependency.

## Storage decision: SQLite

SQLite is embedded and requires no separate database server. It is
appropriate for local metadata, symbol lookup, and indexed text search.

Evaluate FTS5 availability in the Python SQLite build before making it a
hard requirement.

## Rendering decision: Rich

Rich is responsible for terminal presentation only. It must not own
document parsing, persistence, or network behavior.

## HTTP decision: HTTPX

HTTPX is used by explicit page and application update commands. It must
not be called by ordinary page-view, search, or listing commands.

## Compatibility note

The project currently targets Python 3.15.2 in the development
environment. Verify that every selected third-party dependency supports
the chosen interpreter version. If a dependency has not released
compatible wheels or has build issues, reassess it rather than weakening
the local-first architecture.
