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
dynamic sections. The original default update set has 50 C++ standard-library
pages. The generated catalog has 2,496 named entries across 111 headers,
excluding experimental/TS routes and library overview pages.
Six representative pages have saved fixtures for projection tests;
the remaining pages use the same conservative generic projection. Existing
parsed documents remain the stored source of truth; no database migration
was needed. This is a curated selection, not complete cppreference coverage.
Add further entity-specific extraction only with representative fixtures
and source-accuracy tests. The Rich page renderer uses C++ syntax
highlighting and provides `--color auto|always|never` controls.

## Quick-reference pages and full-library coverage (next)

Spec: `docs/quick-reference-spec.md`. Defects:
`docs/audit-2026-11-manual-quality.md`.

-   [x] M1 Real cppreference fixtures (class, algorithm, multi-name page,
    member function, utility, type trait, concept, constant, header page)
    and accuracy tests.
-   [x] M2 Parser fidelity (`t-li1`, declaration overload numbers and
    standard tags, nested revision tables, `t-lines`, noise filtering).
-   [x] M3 Quick-card projection and renderer (short default page,
    hidden-content hint lines, card-budget tests).
-   [x] M4 Storage format version, purpose and alias columns, migration.
-   [x] M5 CLI UX: `--full`, `--example`, ranked search with purpose
    lines, aliases, better not-found, broken-pipe handling.
-   [x] M6 Generated catalog for the whole `/cpp/` standard library and
    `cxxman header <name>` listings.
-   [x] M7 Scale ingestion: resilient per-page crawler; prebuilt database
    distribution only after owner approval (licensing).
-   [x] M8 Validation sweep across stored pages; `docs/coverage-report.md`.

M1 status: complete (2026-11-21). The nine real content-area fixtures are
reproducible with `tools/download_real_fixtures.py` and carry cppreference
CC-BY-SA 3.0 / GFDL attribution.

M2 status: complete (2026-11-21). Real-page parser tests pass for the
numbered description, declaration metadata, nested revision rows,
multi-name tables, `.t-lines` names, and parser-side noise filtering. A
backward-compatible document format 2 migration adds `format_version`;
existing records remain readable as version 1.

M3 status: complete (2026-11-21). Real `vector` invalidation appears in
WATCH OUT; quick cards include purpose, header, annotated declarations,
selected source details, hidden-content hints, and a source footer. Empty
sections are omitted, and pager use is skipped when the card estimate fits.
The 100-column tests enforce 30 lines for a function and 42 for a class.
CLI `--full` and `--example` dispatch remain M5 work.

M4 status: complete (2026-11-22). SQLite record format 3 stores purpose
and aliases, migrates older schemas transactionally while preserving their
format versions, and shows an update hint for pages parsed by an older
format. Legacy content remains readable until refreshed.

M5 status: complete (2026-11-22). The CLI now shows full stored documents
or complete source examples on request, ranks bounded search results by
name/alias, prefix, purpose, then body, and prints header/purpose metadata.
Cached aliases resolve to their owning page and aliases from the catalog
are accepted by update. Not-found guidance includes close name/alias matches
and detects header names. Broken pipes exit quietly. Existing FTS indexes
are rebuilt transactionally with dedicated purpose and alias fields.

M6 status: complete (2026-11-22). The generated, lazily loaded catalog has
2,496 named entries across 111 headers, excludes experimental/TS routes and
library overview pages, and keeps the original 50 pages as the default update
set. `cxxman header <name>` and header-name shortcuts show grouped rows with
source purpose and aliases. The generated catalog and maintainer builder
retain source routes and attribution.

M7 status: complete (2026-11-22). A no-argument `update` selects the full
catalog and commits each successful page independently. The updater rate
limits requests, retries transient network/HTTP failures with backoff,
continues after per-page failures, and reports the outcome. `update --resume`
skips pages already stored. Targeted updates still work. Prebuilt data
distribution remains out of scope pending the owner approval required by
the spec's licensing decision.

M8 status: complete (2026-11-22). `tools/validate_coverage.py` performs a
read-only, one-page-at-a-time sweep and renders each quick card at a fixed
width. The latest `docs/coverage-report.md` covers all 2,167 unique routes in
the 2,496-name catalog and reports zero page/category findings. Four pages
have no source purpose sentence; their NAME lines show only the title. There
are no missing SYNOPSIS sections where the source contains a declaration,
duplicate data rows, concatenated member names, or card-budget overruns. The
local database has no records outside the generated catalog.

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
release steps. Release `v0.2.0` packages the 50-page catalog, manual
renderer, application updater, and Windows setup executable. The earlier
`v0.1.0` release remains available as a historical release.
