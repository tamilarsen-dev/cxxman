# Development Roadmap

## Phase 0 --- Project foundation

-   [x] Choose Python and local-first architecture.
-   [x] Initialize Git repository.
-   [x] Create Python virtual environment.
-   [ ] Create package metadata and CLI entry point.
-   [ ] Verify editable installation and command execution.

## Phase 1 --- CLI skeleton

Commands:

-   `cxxman --help`
-   `cxxman --version`
-   `cxxman vector` (initially reports that the page is not yet
    available)

Acceptance: predictable argument parsing, exit codes, and useful errors.

## Phase 2 --- Document model

Define typed structures for:

-   Document metadata.
-   Heading.
-   Paragraph.
-   Code block.
-   List.
-   Table.
-   Reference/link.

Acceptance: construct representative documents in tests without HTML or
network access.

## Phase 3 --- Parser

-   Save representative cppreference HTML fixtures.
-   Parse fixtures into the document model.
-   Preserve meaningful structure.
-   Add tests for headings, paragraphs, lists, tables, signatures, and
    code.
-   Handle missing or changed HTML structures gracefully.

Acceptance: parser tests run fully offline.

## Phase 4 --- Renderer

-   Render headings and paragraphs.
-   Render code with syntax highlighting.
-   Render lists and tables.
-   Add terminal-width handling and pagination.

Acceptance: fixture documents are readable in PowerShell.

## Phase 5 --- Local storage and search

-   Define SQLite schema.
-   Store page metadata and structured content.
-   Add exact symbol lookup.
-   Add indexed full-text search if supported.
-   Load one page at a time.

Acceptance: show and search operations work offline.

## Phase 6 --- Documentation ingestion

-   Add explicit download/update command.
-   Fetch source HTML with HTTPX.
-   Parse once and persist structured output.
-   Use transactions or staging so failed updates do not destroy valid
    cached pages.
-   Record source and update metadata.

Acceptance: after download, page viewing works with the network
disconnected.

## Phase 7 --- Quality and performance

-   Add CLI integration tests.
-   Measure startup, lookup, search, rendering, and memory.
-   Test missing pages, corrupt records, and network failures.
-   Package and test the Windows command.

Acceptance: documented performance measurements and reliable offline
behavior.

## Release boundary

The MVP ends after Phase 6's core path works and the critical acceptance
criteria in `mvp.md` pass. Phase 7 hardens and measures the
implementation.
