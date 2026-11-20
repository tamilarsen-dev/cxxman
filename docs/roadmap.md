# Development Roadmap

## Phase 0 --- Project foundation

-   [x] Choose Python and local-first architecture.
-   [x] Initialize Git repository.
-   [x] Create Python virtual environment.
-   [x] Create package metadata and CLI entry point.
-   [x] Verify editable installation and command execution.

## Phase 1 --- CLI skeleton

Commands:

-   `cxxman --help`
-   `cxxman --version`
-   `cxxman vector` (initially reports that the page is not yet
    available)

Acceptance: predictable argument parsing, exit codes, and useful errors.

Status: complete. Editable installation, help, version, and the missing
topic response were verified in the project virtual environment.

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

Status: complete. The typed model and its offline construction tests are
in place.

## Phase 3 --- Parser

-   Save representative cppreference HTML fixtures.
-   Parse fixtures into the document model.
-   Preserve meaningful structure.
-   Add tests for headings, paragraphs, lists, tables, signatures, and
    code.
-   Handle missing or changed HTML structures gracefully.

Acceptance: parser tests run fully offline.

Status: complete. Saved HTML fixtures verify supported structures and
graceful fallback behavior without network access.

## Phase 4 --- Renderer

-   Render headings and paragraphs.
-   Render code with syntax highlighting.
-   Render lists and tables.
-   Add terminal-width handling and pagination.

Acceptance: fixture documents are readable in PowerShell.

Status: complete. Rendering covers supported document blocks, terminal
width, and pagination for terminal output.

## Phase 5 --- Local storage and search

-   Define SQLite schema.
-   Store page metadata and structured content.
-   Add exact symbol lookup.
-   Add indexed full-text search if supported.
-   Load one page at a time.

Acceptance: show and search operations work offline.

Status: complete. SQLite persistence, exact topic lookup, bounded search,
and listing APIs are covered by offline tests.

## Phase 6 --- Documentation ingestion

-   Add explicit download/update command.
-   Fetch source HTML with HTTPX.
-   Parse once and persist structured output.
-   Use transactions or staging so failed updates do not destroy valid
    cached pages.
-   Record source and update metadata.

Acceptance: after download, page viewing works with the network
disconnected.

Status: complete. Explicit updates, local read commands, and failed-update
preservation are covered by the full test suite.

## Phase 7 --- Quality and performance

-   [x] Add CLI integration tests.
-   [x] Measure startup, lookup, search, rendering, and memory.
-   [x] Test missing pages, corrupt records, and network failures.
-   [x] Package and test the Windows command.

Acceptance: documented performance measurements and reliable offline
behavior.

Status: complete. The full suite passes, the installed Windows command
works, and a synthetic-corpus baseline is recorded in
`docs/performance.md`.

## Release boundary

The MVP ends after Phase 6's core path works and the critical acceptance
criteria in `mvp.md` pass. Phase 7 hardens and measures the
implementation.

## Post-MVP direction

The initial semantic manual-page path is implemented: a loaded generic
document is projected deterministically at read time and rendered with
dynamic sections. The default download catalog has 50 C++ standard-library
pages. Six representative pages have saved fixtures for projection tests;
the remaining pages use the same conservative generic projection. Existing
parsed documents remain the stored source of truth; no database migration
was needed. This is a curated selection, not complete cppreference coverage.
Add further entity-specific extraction only with representative fixtures
and source-accuracy tests. The Rich page renderer uses C++ syntax
highlighting and provides `--color auto|always|never` controls.

## Release preparation

-   [x] Improve first-run guidance, topic discovery, search feedback, and
    update progress output.
-   [x] Prepare standalone builds and per-user installers for Windows
    x64, Linux x64, Intel macOS, and Apple Silicon macOS.
-   [x] Add a cross-platform GitHub Actions workflow that builds on pushes
    and pull requests, then publishes tagged releases with native
    executable smoke checks and SHA-256 checksums.
-   [x] Publish and verify release `v0.1.0` with four native platform
    archives and SHA-256 checksums.
-   [x] Add explicit application release checks and self-update for
    standalone builds, with SHA-256 verification.
-   [x] Replace the Windows script installer with a per-user Inno Setup
    executable and generated uninstaller; add CI install/remove smoke test.

The source repository and standalone downloads are public at
<https://github.com/tamilarsen-dev/cxxman>. `docs/release.md` describes the
platform archives, install flows, application updates, and maintainer
release steps. The updated setup and updater will reach users in a future
tagged release; published `v0.1.0` remains unchanged.
