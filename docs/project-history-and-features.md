# Project History and Feature Reference

Last updated: 2026-11-22

This document brings together the project history, released capabilities,
current source-tree features, validation results, and links to the detailed
design and operating documents. The latest published release is `v0.3.1`.

## Project at a glance

`cxxman` is a cross-platform terminal reference for C++ standard-library
entities, inspired by Unix manual pages. It downloads and parses reference
pages only when the user explicitly runs an update. Page viewing, search,
listing, and header discovery use local SQLite data and work offline.

The project has two relevant snapshots:

| Snapshot | What it includes |
|---|---|
| Published release `v0.3.1` | Windows x86 and x64, Linux x64, Intel macOS, and Apple Silicon builds; Windows per-user installers; the generated catalog; quick-card and discovery improvements; application update commands. |
| Current source tree (2026-11-22) | The same released capabilities, with 2,496 catalog names across 111 headers and 2,167 distinct cppreference routes; resumable ingestion; a coverage report with no page/category findings; and 108 passing local tests. |

The generated catalog is a broad route-coverage goal for C++ standard-library
reference material under cppreference `/cpp/`. It does not mean every
technical fact on every page has been independently reviewed. Experimental
and TS routes, C pages, language-reference pages, compiler-support pages,
and pure library-overview pages are excluded according to the project scope.

## History from the beginning

### 1. Project foundation and CLI skeleton

The first milestone established the Python package, virtual environment,
Git repository, package metadata, and `cxxman` command. The initial command
surface provided help and version output and a clear response when a page was
not yet available.

### 2. Structured documents and offline parser

The generic document model was added for source metadata and ordered content
such as headings, paragraphs, code blocks, lists, tables, and references.
The cppreference HTML parser turns downloaded source into that model. Saved
fixtures let parser behavior be exercised without network access. This model
remains the preserved source representation used by the manual projection and
full-document view.

### 3. Terminal rendering, storage, and search

Rich rendering was added for readable terminal output, syntax-highlighted C++
code, tables, terminal width, and pagination. SQLite persistence made cached
pages available locally. Exact lookup, bounded search, and page listing were
added without loading the corpus into memory. FTS5 is used when available in
the active SQLite build.

### 4. Explicit ingestion and local-first behavior

HTTPX downloads and HTML parsing were placed on the explicit update path.
Parsed documents and source metadata are saved locally. Page display and
search do not fetch network content or parse HTML. Update writes preserve
valid cached data when a page fails, and the current full-catalog updater
commits successful pages independently so one failure does not stop the
remaining work.

### 5. Quality and performance work

The project added CLI/storage/parser tests, failure handling, and documented
performance strategy and measurements. Page loading remains bounded to one
page at a time; search and listing have result limits. Performance notes are
in [`performance.md`](performance.md).

### 6. First public releases

Release `v0.1.0` was the first historical cross-platform release. Release
`v0.2.0` added SHA-256 checksum files, application release checking and
self-update for standalone builds, and a Windows per-user setup executable
with a generated uninstaller. Release `v0.3.0` bundled the generated topic
catalog in standalone builds. Release `v0.3.1` added Windows x86 binaries
and a matching setup executable alongside Windows x64. CI tests both
Windows installers and their generated uninstallers. The app bundles topic
metadata, while users download cppreference page content explicitly. macOS
builds are not signed or notarized.

### 7. Manual-page quality audit and quick-card direction

A review of the early manual output found that generic article-order output
was too long and could lose source meaning: real markup exposed missing
purpose sentences, declaration annotations, nested revision information,
member-name boundaries, and invalidation details. Synthetic fixtures alone
had not exercised enough real cppreference markup.

The product direction was therefore made explicit: show a concise default
quick card built by selecting source-backed facts, while keeping the complete
stored document accessible with `--full`. Brevity must come from hiding
content with a hint, never from rewriting or cutting source sentences or
examples. The audit is in
[`audit-2026-11-manual-quality.md`](audit-2026-11-manual-quality.md), and the
approved target format and scope are in
[`quick-reference-spec.md`](quick-reference-spec.md).

### 8. M1–M8 implementation (2026-11-21 to 2026-11-22)

| Milestone | Work completed |
|---|---|
| M1: real source fixtures | Added attributed, trimmed cppreference fixtures spanning classes, algorithms, multi-name pages, member functions, utilities, traits, concepts, constants, and headers. |
| M2: parser fidelity | Preserved `t-li1` descriptions, declaration numbering and standard tags, nested revision tables, `t-lines`, and member-name boundaries; filtered page chrome more reliably. |
| M3: quick cards | Added deterministic, source-backed selection for the short default view, dynamic omission of empty sections, hidden-content hints, source footer, and card-budget-aware pager behavior. |
| M4: stored format | Added document format version 3, purpose and alias metadata, compatibility/migration behavior, and a refresh hint for older parser data. |
| M5: CLI discovery and detail | Added `--full`, `--example`, ranked purpose-aware search, cached/catalog alias resolution, close-match and header guidance, and quiet handling of closed output pipes. |
| M6: generated catalog | Replaced the small hand-maintained catalog with generated catalog data and added `cxxman header <name>` listings with grouped names, purposes, and aliases. |
| M7: scale ingestion | Added full-catalog updates, per-page commits, request pacing, retries/backoff, continuation after page failures, failure reporting, and `update --resume`. |
| M8: coverage sweep | Added a read-only coverage tool that reads one page at a time, renders cards at a fixed width, and reports structural and budget issues. |

The M1–M8 suite history recorded 75 tests after M3, 78 after M4, 88 after
M5, 94 after M6, 99 after M7, and 104 after M8. These are milestone-time
results, not a claim that the full suite was rerun after every later
working-tree edit.

### 9. Catalog cleanup and latest coverage (2026-11-22)

Catalog generation was corrected to exclude experimental/TS routes and pure
library overview pages while retaining the substantive Library
feature-test-macros reference. Three stale cached pages (`array`, `vector`,
and `vector::push_back`) were refreshed. The local cache was then brought
into alignment with the generated catalog; legacy out-of-scope cache rows
were removed.

The current catalog contains 2,496 names across 111 headers and maps those
names to 2,167 distinct routes. All 2,167 routes are present in the local
database, with no stored page outside the catalog. The latest report found:

- 0 parse failures or pages needing refresh.
- 0 missing SYNOPSIS sections where a source declaration exists.
- 0 duplicate table rows, concatenated member names, or card-budget overruns.
- 4 pages with no source purpose sentence; the title is shown without an
  invented purpose.

These checks validate stored structure, route coverage, and card budgets;
they do not prove every displayed technical fact against current live source.
See [`coverage-report.md`](coverage-report.md).

## Feature reference

### Commands and options in the current source tree

| Command or option | Behavior |
|---|---|
| `cxxman --help`, `cxxman --version` | Show command help or application version. |
| `cxxman <topic>` | Show a concise quick card from the local database. `std::` prefixes and known aliases are accepted. |
| `cxxman <topic> --full` | Show the complete locally stored parsed reference document. |
| `cxxman <topic> --example` | Show the source example section in full. |
| `cxxman --color auto\|always\|never <topic>` | Choose terminal color behavior; automatic mode follows terminal support. |
| `cxxman search <words>` | Search local cached pages with bounded, deterministic ranking and purpose metadata. |
| `cxxman header <name>` | List known entities from a standard header, grouped with purpose/alias information when available. |
| `cxxman <header-name>` | Use the header-name shortcut to discover entities when the name is a header rather than an entity. An entity with the same name takes precedence. |
| `cxxman list` | List pages in the local database. |
| `cxxman topics` | List catalog topics and their source/header metadata. |
| `cxxman update` | Explicitly fetch/update the generated full catalog in the current source tree. |
| `cxxman update --resume` | Continue the full-catalog update while skipping pages already stored. |
| `cxxman update <topic ...>` | Explicitly fetch selected catalog topics; aliases resolve to their source page. |
| `cxxman check-updates` | Explicitly check GitHub for a newer application release. |
| `cxxman self-update` | Download and install a verified release for standalone builds. Python installations update through pip or source checkout. |

The standalone `v0.3.1` release includes the generated catalog. It does not
bundle downloaded page content; `cxxman update` stores those pages in the
user's local database.

### Quick-card information

The default card prioritizes the entity name and source purpose, header,
declarations with source annotations, selected parameters and return
information, complexity, relevant pitfalls, a short source example,
related names, and a source link. Sections are included only when supported
by stored source data. When useful source material is hidden to fit the
card, the output indicates how to request `--full` or `--example`.

Extraction is deterministic and does not call an LLM or remote summarizer.
The generic parsed document remains stored so the concise projection does
not discard detailed source content. Unknown or ambiguous facts are retained
for full view or omitted from the card rather than guessed.

### Architecture and reliability features

- **Local-first read path:** page view, search, list, topics, and header
  discovery use local catalog/database data. Only explicit update and
  application-update commands use the network.
- **Structured source preservation:** parsed headings, paragraphs, code,
  lists, tables, references, declaration metadata, purpose text, aliases,
  and source attribution are retained in the document/storage model.
- **SQLite storage:** parameterized queries, versioned stored-document
  format, compatibility handling, indexes, bounded result sets, and FTS5
  where supported by the active SQLite build.
- **Resilient updates:** rate limiting, retry/backoff for transient errors,
  per-page commits, continuation after a page failure, failure summaries,
  and resumable full-catalog ingestion.
- **Terminal output:** Rich layout, C++ syntax highlighting, configurable
  color, terminal-width-aware wrapping, pagination for long output, and
  quiet broken-pipe handling for pipelines.
- **Cross-platform app distribution:** Windows x86 and x64, Linux x64,
  Intel macOS, and Apple Silicon packages in `v0.3.1`; per-user setup and
  uninstall on both Windows architectures; checksum sidecars; and CI
  packaging/install smoke tests.
- **Maintainer tooling:** catalog builder, saved-fixture downloader, and
  read-only coverage validator. The coverage validator does not update the
  database or fetch pages.

## Current limits and deferred work

- The current generated catalog covers routes in the defined cppreference
  `/cpp/` scope; it does not include C language pages, C++ language-reference
  pages, experimental/TS pages, or compiler-support tables.
- Route presence and parser/card checks are not equivalent to a manual
  technical review of every page. Source accuracy remains the priority for
  future expansion of entity-specific extraction.
- A prebuilt, bundled, or downloadable database distribution is not
  implemented. It needs explicit owner approval and a plan for the
  cppreference CC-BY-SA 3.0 / GFDL attribution and share-alike terms.
- macOS binaries are not signed or notarized. Windows ARM and Linux ARM
  builds are not currently provided.

## Documentation index

| Document | What it contains |
|---|---|
| [`user-manual.md`](user-manual.md) | Install, commands, page reading, search, local data, and troubleshooting. |
| [`roadmap.md`](roadmap.md) | Original implementation phases, release history, and M1–M8 status. |
| [`continuation.md`](continuation.md) | Current handoff, Git/release context, latest catalog and coverage state. |
| [`quick-reference-spec.md`](quick-reference-spec.md) | Approved quick-card format, discovery behavior, catalog scope, and milestones. |
| [`audit-2026-11-manual-quality.md`](audit-2026-11-manual-quality.md) | Quality issues found in the earlier manual output and their resolutions/status. |
| [`coverage-report.md`](coverage-report.md) | Latest machine-generated scan of stored-page structure and card budgets. |
| [`manual-reference-audit.md`](manual-reference-audit.md) | Earlier architecture audit and original integration recommendations. |
| [`architecture.md`](architecture.md) and [`project-structure.md`](project-structure.md) | Runtime pipeline, module responsibilities, and package layout. |
| [`developer-guide.md`](developer-guide.md) and [`technology-stack.md`](technology-stack.md) | Development setup, contribution practices, and technology decisions. |
| [`release.md`](release.md) and [`release-readiness.md`](release-readiness.md) | Published build targets, install/update flows, and release verification. |
| [`performance.md`](performance.md) | Local-first performance principles and recorded measurements. |
| [`mvp.md`](mvp.md) | Initial product scope and MVP acceptance criteria. |

