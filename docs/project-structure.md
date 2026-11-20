# Project Structure

Conceptual package layout (the current source adds the modules listed below):

``` text
cxxman/
│
├── pyproject.toml
├── README.md
├── .gitignore
│
├── src/
│   └── cxxman/
│       ├── __init__.py
│       ├── __main__.py
│       ├── cli.py
│       │
│       ├── core/
│       │   ├── __init__.py
│       │   ├── document.py
│       │   └── search.py
│       │
│       ├── parser/
│       │   ├── __init__.py
│       │   └── html_parser.py
│       │
│       ├── storage/
│       │   ├── __init__.py
│       │   └── database.py
│       │
│       └── ui/
│           ├── __init__.py
│           └── renderer.py
│
├── tests/
│   ├── test_parser.py
│   ├── test_search.py
│   └── test_storage.py
│
└── data/
```

## Module responsibilities

  Path                      Responsibility
  ------------------------- --------------------------------------------
  `__main__.py`             Module execution entry point
  `cli.py`                  CLI argument parsing and command dispatch
  `core/catalog.py`         Curated download topics and source metadata
  `core/document.py`        Document metadata and typed content-block structures
  `core/manual.py`          Read-time manual-page projection
  `core/search.py`          Search service and result handling
  `core/update.py`          Explicit remote fetch, parsing, and storage flow
  `core/app_updates.py`     GitHub release checks and verified self-update
  `parser/html_parser.py`   HTML to structured document conversion
  `storage/database.py`     SQLite schema and persistence
  `ui/renderer.py`          Rich manual/document rendering, syntax highlighting, and pagination
  `installer/cxxman.iss`    Windows per-user setup and generated uninstaller
  `tests/`                  Automated tests
  `data/`                   Local database and documentation data

## Package rules

-   Keep `__main__.py` minimal.
-   Keep CLI parsing separate from application services.
-   Use `__init__.py` files to make package boundaries explicit.
-   Do not put downloaded documentation, generated databases, or caches
    under `src/`.
-   Do not commit `.venv/` or generated local documentation data.
-   Add modules only when they have a clear responsibility.
