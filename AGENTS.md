# AGENTS.md

Project instructions for Codex and other coding agents working in `cxxman`.

## 1. Project mission

Build `cxxman`, a terminal-based C++ documentation viewer inspired by Linux `man`.

The intended user experience is:

```powershell
cxxman vector
cxxman string
cxxman sort
cxxman search unordered_map
```

Documentation must be readable directly in the terminal. Do not open a browser or GUI when displaying a page.

The project is being developed on Windows using PowerShell and Python 3.15.2. Keep the implementation portable where practical.

## 2. Product principles

1. **Local-first:** ordinary page viewing and search use local data only.
2. **Fast read path:** opening a cached page must not perform HTTP requests or parse source HTML.
3. **Readable output:** preserve document structure instead of flattening a page into one long string.
4. **Small dependencies:** prefer the Python standard library when it is sufficient.
5. **Incremental delivery:** implement one coherent milestone at a time.
6. **Testable boundaries:** parsing, storage, search, CLI, and rendering must be independently testable.
7. **No speculative complexity:** do not add abstractions, dependencies, background services, or features without a concrete MVP need.

## 3. Local-first architecture

There are two separate execution paths.

### Read path: offline

```text
User command
    -> CLI
    -> local lookup
    -> SQLite / local document storage
    -> structured document model
    -> terminal renderer
    -> terminal
```

For commands such as `cxxman vector` and `cxxman search vector`:

- Never make an HTTP request.
- Never download source HTML.
- Never parse HTML.
- Read only the requested page or a bounded set of search results.
- Do not load the entire documentation corpus into memory.

### Ingestion/update path: network allowed

```text
Explicit update command
    -> HTTP client
    -> downloaded HTML
    -> HTML parser
    -> structured document model
    -> local storage and search index
```

Network access is allowed only for an explicit command such as `cxxman update` or a future explicit download command.

Parse HTML only during ingestion or update. Persist the parsed representation so the read path does not need the original HTML.

A failed update must not destroy a previously valid local page. Use transactions or a staging-and-commit strategy.

## 4. Performance requirements

Treat these as architectural requirements:

- No network access on page-view or search commands.
- No HTML parsing on page-view or search commands.
- Use SQLite indexes for exact symbol lookup and search.
- Evaluate SQLite FTS5 for full-text search; check whether the active Python SQLite build supports it before depending on it.
- Load one page at a time.
- Bound search result counts.
- Avoid scanning the full corpus in Python for each query.
- Avoid expensive work during CLI startup.
- Use pagination for long terminal output.
- Do not introduce caching layers on top of the local database unless measurements justify them.

Measure rather than guess. Use `timeit` for focused operations and `cProfile` for profiling. Record the corpus size and machine details when reporting measurements. Do not claim performance targets have been met without measurements.

## 5. Technology stack

Use the following planned stack unless a compatibility issue or measured reason justifies a change:

| Area | Technology | Rules |
|---|---|---|
| Language | Python 3.15.2 development interpreter | Verify the actual interpreter used by the project venv |
| CLI | `argparse` | Standard library; do not add Typer for the MVP |
| HTML parsing | BeautifulSoup4 | Used only during ingestion/update |
| HTTP | HTTPX | Used only by explicit network commands |
| Storage | SQLite via `sqlite3` | Prefer standard-library SQLite |
| Terminal UI | Rich | Rendering only; no networking or persistence |
| Tests | Pytest | Unit and integration tests |
| Profiling | `timeit`, `cProfile` | Standard-library tools |

### Dependency compatibility

Python 3.15.2 is the project's selected development interpreter. Before adding or upgrading third-party packages:

1. Check that the package supports the active Python version.
2. Prefer a compatible wheel over an unnecessary native build.
3. Install into the project virtual environment, never globally.
4. If a required dependency is not compatible, stop and report the issue with options. Do not silently downgrade Python or replace the agreed architecture.

Keep runtime dependencies minimal. Do not add a package merely because it is familiar.

## 6. Target project structure

Use this structure as the intended direction. Do not create every module before it is needed.

```text
cxxman/
├── AGENTS.md
├── pyproject.toml
├── README.md
├── .gitignore
├── docs/
│   ├── mvp.md
│   ├── architecture.md
│   ├── performance.md
│   ├── technology-stack.md
│   ├── project-structure.md
│   └── roadmap.md
├── src/
│   └── cxxman/
│       ├── __init__.py
│       ├── __main__.py
│       ├── cli.py
│       ├── core/
│       │   ├── __init__.py
│       │   ├── document.py
│       │   └── search.py
│       ├── parser/
│       │   ├── __init__.py
│       │   └── html_parser.py
│       ├── storage/
│       │   ├── __init__.py
│       │   └── database.py
│       └── ui/
│           ├── __init__.py
│           └── renderer.py
├── tests/
└── data/
```

Keep downloaded documentation and generated databases outside `src/`. Do not commit `.venv/`, local documentation databases, or downloaded corpus data.

## 7. Module responsibilities

- `cli.py`: parse arguments, dispatch commands, and translate expected errors into useful terminal messages. Keep domain logic out of the CLI.
- `core/document.py`: define typed document and content-block structures.
- `core/search.py`: implement search behavior over the local index.
- `parser/html_parser.py`: convert source HTML into structured document objects. It must not render to the terminal or write arbitrary database queries.
- `storage/database.py`: own SQLite schema, connection handling, transactions, and persistence operations.
- `ui/renderer.py`: render document objects using Rich. It must not fetch network content or query the database directly.
- `__main__.py`: remain a minimal module entry point.

Use clear interfaces between these modules. Avoid circular imports.

## 8. Document model requirements

Do not represent an entire documentation page as one plain-text string.

The model should preserve page metadata and an ordered sequence of typed content blocks. Start with only the block types required by real fixtures:

- Heading
- Paragraph
- Code block / function signature
- Ordered or unordered list
- Table
- Link/reference where useful

A document should support metadata such as:

- Stable page identifier / canonical topic
- Display title
- Source URL
- Header name, if applicable
- Source/update metadata
- Ordered content blocks

Use Python type hints. Prefer dataclasses or similarly lightweight standard-library structures unless there is a demonstrated reason for another dependency.

## 9. HTML parser requirements

- Build parser tests from saved HTML fixtures; parser tests must not require internet access.
- Parse semantic HTML elements and preserve their order.
- Preserve code blocks, headings, paragraphs, lists, and tables as distinct structures.
- Strip navigation, menus, sidebars, footers, and unrelated page chrome where possible.
- Do not assume every cppreference page has identical HTML.
- Handle missing or changed elements gracefully.
- Do not use regex as the primary HTML parser.
- Keep source-specific HTML handling inside the parser module.
- Record unsupported structures as known limitations rather than silently corrupting the whole page.

## 10. Storage and search requirements

- Use SQLite through Python's `sqlite3` module.
- Define schema creation/migration behavior explicitly.
- Use parameterized SQL for all values.
- Use transactions for writes and updates.
- Add indexes for canonical topic and other frequently queried fields.
- Consider FTS5 for full-text search only after verifying support.
- Keep search results bounded and deterministic.
- Do not fetch or parse HTML during search.
- Avoid loading all page bodies for a metadata-only search.

## 11. Terminal rendering requirements

Use Rich for terminal presentation.

The renderer should:

- Respect terminal width.
- Distinguish headings from body text.
- Render paragraphs with readable spacing.
- Render lists with indentation.
- Render tables without destroying their row/column relationships.
- Render code and signatures in readable code blocks, with syntax highlighting where practical.
- Support pagination for long documents.
- Degrade gracefully when output is piped or terminal color is unavailable.

Do not use color as a substitute for structure. Readability comes first.

## 12. CLI behavior

Use `argparse`.

Planned MVP commands:

- `cxxman --help`
- `cxxman --version`
- `cxxman <topic>`: display a local page only.
- `cxxman search <query>`: search the local index only.
- `cxxman list`: list locally available pages.
- `cxxman update`: explicit network-enabled ingestion/update operation.

If a page is not available locally, report that clearly and tell the user which explicit update command can obtain it. Do not silently fall back to the network.

Return sensible exit codes. Keep output useful and concise.

## 13. Coding conventions

- Use Python type hints for public functions and important data structures.
- Follow PEP 8.
- Use descriptive names; avoid clever abbreviations.
- Keep functions focused and reasonably small.
- Prefer explicit control flow over dense one-liners.
- Use `pathlib.Path` for filesystem paths.
- Use context managers for files, database connections/cursors, and HTTP resources.
- Use parameterized SQL.
- Catch specific exceptions. Do not hide errors with broad `except Exception` unless re-raising or handling at a true application boundary.
- Do not use global mutable state for application services.
- Do not add comments that merely repeat the code.
- Add docstrings to public modules/classes/functions where they clarify contracts.
- Keep Windows paths and shell commands correct for PowerShell; avoid hard-coded user-specific paths.
- Do not modify unrelated files or reformat the entire project without need.

## 14. Testing requirements

Use Pytest.

Tests should be deterministic and must not rely on live network access unless explicitly marked as a separate integration test.

Minimum expectations:

- Unit tests for document model behavior.
- Parser tests using local HTML fixtures.
- Storage tests using temporary SQLite databases.
- Search tests for exact matches, partial matches, no results, and bounded results.
- Renderer tests for the important block types where practical.
- CLI tests for help, version, missing pages, search, and exit codes.
- A test proving that page viewing and search do not make HTTP requests.
- Update tests for network failures and preservation of the last valid local version.

Run the relevant tests after each change, then run the full test suite before completing a milestone. Do not claim tests passed unless they were actually run.

## 15. MVP scope and acceptance criteria

The MVP is a usable terminal application with a small initial set of C++ reference pages. It is not a complete cppreference mirror.

MVP commands:

| Command | Behavior |
|---|---|
| `cxxman vector` | Display a locally stored page; no network |
| `cxxman search vector` | Search local SQLite index; no network |
| `cxxman list` | List local pages; no network |
| `cxxman update` | Explicitly download, parse, and store documentation |
| `cxxman --help` | Show help |
| `cxxman --version` | Show version |

MVP is complete when:

- CLI help and version work.
- A locally stored page can be displayed.
- Page viewing works offline and performs no HTTP request.
- HTML is parsed only during ingestion/update.
- Headings, paragraphs, lists, tables, signatures, and code examples are represented structurally.
- SQLite supports local lookup and search.
- Only the requested page and bounded search results are loaded.
- Long output supports pagination.
- Missing pages produce a useful message without network fallback.
- Critical behaviors are covered by automated tests.

Out of scope for MVP:

- Full cppreference mirror.
- GUI or browser rendering.
- JavaScript execution.
- Automatic background updates.
- Multiple documentation providers.
- Advanced fuzzy search/autocomplete.
- Perfect reproduction of every source website layout.

## 16. Required agent workflow

Work in small, reviewable milestones.

Before changing code:

1. Inspect the existing repository and read `README.md` and the relevant files under `docs/`.
2. Check the current Git status. Do not overwrite user changes.
3. Identify the smallest coherent implementation step.
4. State the files you intend to create or modify and why.

While working:

1. Implement only the agreed milestone.
2. Add or update tests with the implementation.
3. Do not create placeholder modules without a current use.
4. Do not add unrelated features.
5. Do not change the technology stack or architecture silently.
6. Do not make network calls from read-path code.

Before finishing:

1. Run relevant tests.
2. Run the full test suite if practical.
3. Run formatting/lint tools only if configured; do not add them without a reason.
4. Inspect `git diff` and `git status`.
5. Summarize implementation, tests run, results, and known limitations.
6. Identify the next smallest milestone, but do not start it without user approval.

If a requirement is ambiguous or a dependency is incompatible with Python 3.15.2, stop and explain the trade-off rather than guessing.

## 17. Current project state

The original Phase 1 CLI skeleton task is complete. The project now has
the document model, parser, renderer, SQLite storage and search, explicit
updates, first-run guidance, and a cross-platform release workflow. The
public repository and the first downloadable release (`v0.1.0`) are live.

For future work:

1. Read `docs/continuation.md`, `docs/roadmap.md`, and the relevant user
   or developer guide before choosing a milestone.
2. Inspect Git status and the current implementation; do not repeat the
   original CLI skeleton task.
3. Keep each change focused, tested, and committed locally.
4. The next planned direction is adding more focused library-function
   manual pages; `std::vector::push_back` is already implemented.
