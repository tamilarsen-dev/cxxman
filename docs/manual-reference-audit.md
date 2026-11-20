# Manual Reference Architecture Audit
**Date:** 2026-11-20
**Scope:** Phase 0 repository audit only. No application behavior or schema was changed.

This is a point-in-time audit of the pre-projection implementation. The
subsequent implementation state is summarized in `docs/roadmap.md` and
`docs/continuation.md`; audit observations below describe the source tree
at the time of the audit.

## 1. Current architecture overview

CXXMAN has two paths with a sound local-first boundary:

- **Update:** `core/update.py` selects known pages from `core/catalog.py`, fetches them with HTTPX, calls `parser/html_parser.py`, then persists the resulting `Document` objects through `storage/database.py`.
- **Read:** `cli.py` calls SQLite lookup/search/list methods; a topic lookup returns one `Document`, and `ui/renderer.py` renders its blocks. Search returns bounded metadata. Ordinary reads do not call HTTP or parse HTML.

The current persisted representation is the generic parsed article, not a manual page. The design document's proposed semantic transformation is not implemented in the inspected source tree.

## 2. Current documentation pipeline

```text
core/catalog.py
  -> core/update.py (HTTPX fetch, explicit update only)
  -> parser/html_parser.py (BeautifulSoup -> ordered generic blocks)
  -> core/document.py (Document + Heading/Paragraph/CodeBlock/ListBlock/Table/Reference)
  -> storage/database.py (JSON blocks + searchable text; transaction; optional FTS5)

cli.py topic dispatch
  -> Database.get_document (one record)
  -> ui/renderer.py render_document (source-order block rendering)
  -> terminal
```

For search, `cli.py` calls `Database.search`, which returns up to 20 metadata rows by default and does not deserialize page bodies. SQLite builds an FTS5 index when available and otherwise uses a bounded-result `LIKE` fallback. `core/search.py` is a thin wrapper around this storage API. There is no separate cache module: SQLite is the local store and cache. `core/update.py` fetches the batch before `save_documents`; the database context transaction preserves previous records when fetch/parse fails before persistence.

## 3. What the inspected output and fixtures show

The tracked parser fixtures are `tests/fixtures/vector.html` and `tests/fixtures/vector_push_back.html`. The first has a short vector introduction, a member-functions heading, a code sample, lists, and an operation/complexity table. The second has two `push_back` declarations, a parameter table, complexity prose, and an example. `test_parser.py` confirms that both overload declarations survive as separate `CodeBlock` values.

The requested five-page comparison is only partly verifiable from repository data:

| Topic | Catalog entry | Tracked source fixture / cached page found | Audit evidence |
|---|---:|---:|---|
| `std::vector` | Yes | `vector.html` | Generic block order and one member/complexity table are tested. |
| `std::vector::push_back` | Yes | `vector_push_back.html` | Two declarations are preserved; parameter/complexity headings and example are present in fixture. |
| `std::sort` | Yes | No | Catalog URL/header are present; no saved page or stored-document corpus is in the repository. |
| `std::string` | Yes | No | Catalog URL/header are present; no saved page or stored-document corpus is in the repository. |
| `std::optional` | No | No | Not yet represented by the catalog, fixture, or repository-local cache. |

The `data/` directory has no tracked or untracked files in the inspected workspace, so there was no local cached corpus to render. No live fetch was made. Therefore, claims about the actual current `sort`, `string`, or `optional` page output would be assumptions; the source fixtures and code path show how those pages would be treated if downloaded.

The output resembles cppreference because `render_document` prints the title and then walks every surviving block in source order. The parser retains source article headings and prose without deciding which information belongs in a concise manual. The renderer has no entity classification, section selection, or semantic grouping. A vector page therefore renders its article sections as parsed rather than as a concise overview of common operations and guarantees.

### Preserve, omit, extract, and retain for detail

- **Preserve in the concise view when supported by source:** header, declarations and overload distinctions, short purpose/behavior, parameter and return behavior, complexity, requirements/exceptions, invalidation/other important guarantees, standards, and source reference.
- **Normally omit from the concise view:** navigation and page chrome, unrelated article subsections, repeated prose, exhaustive tutorial content, and long examples that do not add a critical qualification. These are projection choices, not parser deletion rules.
- **Requires semantic extraction:** entity kind; declaration grouping; parameter-to-description association; return behavior; section meaning; member operation grouping; standards history; and identification of which guarantees apply to which overload or operation.
- **Keep available in the generic/detail representation:** article prose, examples, tables, links, and less common source sections, including facts not yet classified reliably. The existing blocks are already persisted and should not be thrown away to achieve a shorter default rendering.

## 4. Concrete limitations and data-accuracy findings

### Parser (`src/cxxman/parser/html_parser.py`)

- `parse_html` removes a fixed list of MediaWiki navigation IDs/classes and then recursively converts selected element types. It handles common `mw-content-text`/`bodyContent` containers and falls back to body content.
- Content is represented as generic blocks; heading text does not retain a parent/child section tree or an explicit relationship to following blocks beyond tuple order.
- For cppreference `t-dcl-begin` tables, `_parse_declarations` emits the first cell of rows marked `t-dcl` or `t-dcl-rev` as a code block. In the push_back fixture this correctly preserves two overload signatures, but it drops the overload marker in the second cell and ignores declaration headers such as “Defined in header <vector>”. Other row classes/markup variants may not match this extraction.
- Ordinary tables are converted to `Table`, which preserves their visible cells and header row but loses HTML links, inline emphasis/code, cell-level metadata, row identifiers, and any semantic relation to a specific declaration. Parameter tables survive as tables, not as structured parameter records.
- Inline code is folded into text. A paragraph that consists solely of one link becomes `Reference`; embedded links are flattened into `Paragraph` text, losing the target. `<pre>` content and declaration rows both become `CodeBlock`, so their role is not typed separately.
- The generic blocks do not themselves encode overload-specific parameters, return values, constraints, exception clauses, standards history, or facts scoped to a member/overload. Such content may be present in text or table cells, but extraction requires interpretation and some structure has already been lost.
- Source-specific selectors correctly remain in the parser; semantic meaning should not be added to the Rich renderer.

### Model, persistence, CLI, and renderer

- `src/cxxman/core/document.py` is a lightweight immutable model with page metadata and six generic block types. It has `header`, `source_url`, and `updated_at`, but no namespace/entity kind, standards, semantic sections, parameter records, or overload records.
- `src/cxxman/storage/database.py` serializes each block type into `content_json` and stores searchable text. It has indexed topic/identifier lookup, optional FTS5, atomic writes, and one-record retrieval. It currently has create-if-absent setup rather than a versioned migration mechanism. A future persisted semantic model would require an explicit compatibility/migration plan; the audit does not recommend changing the schema in the first implementation step.
- `src/cxxman/cli.py` currently owns command dispatch and directly chooses DB lookup and `render_document`. It should remain a dispatcher; semantic parsing/projection rules do not belong in it.
- `src/cxxman/ui/renderer.py` renders generic blocks with Rich, supports width-aware wrapping and paging, and has no semantic/manual section API. It is a useful presentation foundation, not the place to classify cppreference source content.
- `src/cxxman/core/catalog.py` is the current curated source of stable topic, identifier, URL, and header metadata. It lists vector, string, sort, and vector::push_back; optional is absent. Header information can therefore come from trusted catalog metadata when available, but should not be guessed for arbitrary pages.

### Accuracy implications

The parser's extraction is demonstrably sufficient for preserving the two push_back declaration strings in the supplied fixture; this is not proof of reliable overload extraction across cppreference variants. The fixture includes signatures with different parameter types and the test asserts both strings independently. It does not verify constraints, return/value semantics, or per-overload behavior. Function return types can be read from a preserved declaration only when its source row was recognized intact; `void` is present in this fixture, but there is no structured return fact. Parameters, complexity, requirements, exceptions, and standard availability are not extracted into their own typed facts by current code.

The vector fixture's complexity table is kept as an ordinary table; the parser does not verify the values, determine coverage, or map each guarantee to a named operation beyond textual adjacency in the table. The fixed skip list includes `noprint`; on real source pages that may hide content categorized that way, so standard/compiler-support material must be checked on fixtures before assuming it remains available. The current fixtures do not cover standards, exception guarantees, requirements/constraints, or invalidation.

Recommended rule for future extraction: only emit a typed semantic fact when a deterministic source pattern and its scope are unambiguous. Keep unclassified content in generic blocks; omit a concise-page fact rather than infer it. Preserve each declaration as a separate item, and associate guarantee/parameter data with an overload only when the source structure establishes that association.

## 5. Proposed target architecture

Reuse the current update, generic document, storage, read-path, and Rich foundations. Add a deterministic semantic stage over `Document` that classifies and extracts a conservative set of facts, then projects those facts into a lightweight manual-page value with metadata and ordered optional sections. A manual renderer consumes that prepared value; the existing document renderer remains useful for a future detailed/source view.

Recommended conceptual flow:

```text
Update: HTML -> existing parser -> generic Document -> existing SQLite storage
Read:   SQLite -> Document -> semantic extraction/projection -> ManualPage -> Rich manual renderer
                                              \-> generic document renderer for detailed/source mode later
```

Initially derive the manual page at read time from the already-loaded one-page `Document`. This avoids duplicate persisted data and a schema migration while the extraction model is small. Measure projection cost before considering storage of a projected result. The transformation must remain local, deterministic, and network-free.

For entity classification, use explicit metadata where available (canonical topic/catalog/page identity) and structural evidence from the document. Keep an `unknown`/unclassified path rather than treating every `CodeBlock` or page title as a reliable function declaration. Projection should include only non-empty sections and use a stable ordering for known manual sections while retaining meaningful order within declarations and overloads. Unknown section/content types should be safely skipped or represented as generic detail content, not crash rendering.

The proposed design's semantic extraction and projection are useful conceptual responsibilities, but they need not become two modules or a section-class hierarchy automatically. Start with one focused core module and a small immutable model only if Phase 1 demonstrates that extraction and selection need a distinct output contract. Keep cppreference HTML selectors in the existing parser, source facts out of Rich formatting, and CLI domain logic out of dispatch.

## 6. Existing components to reuse

- `src/cxxman/core/document.py`: immutable typed generic source representation.
- `src/cxxman/parser/html_parser.py`: current HTML normalization, chrome filtering, ordered block extraction, and declaration-table special case; improve only when a concrete fixture reveals a needed source structure.
- `src/cxxman/core/catalog.py`: stable supported-topic metadata including known headers and URLs.
- `src/cxxman/core/update.py`: explicit network boundary and fetch-parse-store sequence.
- `src/cxxman/storage/database.py`: local page persistence, JSON block serialization, exact lookup, indexed search, bounded metadata retrieval, and transactional updates.
- `src/cxxman/cli.py`: existing topic dispatch and offline behavior to preserve while routing a page to a new projection/render path.
- `src/cxxman/ui/renderer.py`: Rich console, terminal width/wrapping, code, table, and pager utilities; reuse presentation primitives where appropriate, but add a manual-page entry point rather than adding source classification here.
- `tests/` and saved fixtures: current offline testing boundaries, update failure tests, and read-path no-HTTP regression test.

## 7. Required code changes by component

“Required” below means required to implement the direction safely, not a request to implement anything during this audit.

| Existing file/component | Current responsibility | Limitation | Proposed modification | Why necessary |
|---|---|---|---|---|
| `src/cxxman/core/document.py` | Generic parsed document and block types | No semantic page/entity/section representation | Add a small typed manual output model only after its first consumers/sections are defined; preserve `Document` unchanged as source | Keeps parsed article data distinct from concise projection data |
| `src/cxxman/core/` (new focused module, e.g. `manual.py`) | No semantic extraction/projection component exists | Current renderer receives raw source-order blocks | Add deterministic classification, conservative fact extraction, and projection over `Document`; separate these functions in the module even if initially colocated | This is the smallest integration point for semantic transformation, with no parser/database rewrite |
| `src/cxxman/ui/renderer.py` | Rich renderer for generic blocks | Cannot render dynamic manual sections/entity-specific compact layouts | Add a manual-page renderer that renders only present prepared sections; retain `render_document` for detail/regression use | Presentation needs a semantic input; rendering should not perform extraction |
| `src/cxxman/cli.py` | argparse and command dispatch; directly connects DB result to generic renderer | Topic display has no projection step | Route loaded `Document` through the manual-page service and renderer; keep lookup, errors, and dispatch behavior stable | Integrates the projection while preserving existing CLI and offline boundaries |
| `src/cxxman/parser/html_parser.py` | HTML to generic block conversion | Some source relationships/metadata are lost (notably declaration row markers and rich table cell structure) | Do not rewrite preemptively. Add focused preservation only when representative fixtures demonstrate that a needed fact cannot be recovered from current blocks | Prevents extraction from becoming guesses without broad parser churn |
| `src/cxxman/storage/database.py` | SQLite schema, JSON block persistence, indexed search | No semantic payload or migration plan; generic content is stored | Keep schema unchanged for initial read-time projection; if later persisting semantic output, add explicit versioned migration and compatibility tests | Avoids redundant storage and unnecessary schema risk until measured/needed |
| `src/cxxman/core/catalog.py` | Known downloadable page IDs, URLs, and headers | No optional entry or semantic kind metadata | Treat existing metadata as the first trusted source for headers; add entity metadata only if source structure is insufficient and catalog growth justifies it | Prevents guessing headers/entity type and supports different entity shapes |
| `tests/test_parser.py`, `tests/test_renderer.py`, `tests/test_cli.py` | Parser, rendering, and CLI regressions | No semantic projection or manual section coverage | Add fixture-backed extraction/projection and dynamic renderer tests; retain existing article rendering and offline command tests | Protects correctness at the new boundary and existing behavior |
| `tests/fixtures/` | Saved vector and push_back HTML samples | No algorithm, string, optional, or typed edge-case samples | Add sanitized/saved representative fixtures before claiming those entities are supported | Ensures extraction is tested against source variation without tests requiring network |

`src/cxxman/core/search.py` and search schema/query code are not on the page projection path and need no change for this milestone. The implementation should not change SQLite schema, HTTP behavior, or FTS search merely to add semantic presentation.

## 8. Risks and trade-offs

- **False semantic precision:** table adjacency or headings alone may not prove that a guarantee applies to a specific overload. Conservative omission is safer than emitting a misleading fact.
- **Fixture coverage:** the repository currently has only vector and push_back parser fixtures, so `sort`, `string`, and `optional` cannot be audited at source-structure level from local artifacts.
- **Parser flattening:** some inline/link/table semantics are already lost in generic blocks. If a required fact cannot be recovered, a targeted parser enhancement and fixture should precede extraction for that fact.
- **Read-time cost:** projection would run on each page open. It avoids schema duplication but has not been benchmarked. Begin with this option and measure before materializing semantic pages.
- **Identity ambiguity:** titles and code blocks alone are weak entity classifiers. Prefer catalog identity/explicit metadata and allow unclassified output.
- **Unknown sections:** a strict renderer registry can accidentally hide useful content; safely preserve or fall back to generic detail output.
- **Schema compatibility:** storing semantic output would require a migration/serialization compatibility plan because storage initialization currently creates tables/indexes but does not version migrations.
- **Stale supporting docs:** `docs/user-manual.md` says a release has not yet been published, while `docs/continuation.md` and `docs/release.md` describe published v0.1.0. This does not affect the manual-reference architecture, but is a verified documentation inconsistency to resolve separately.

## 9. Recommended implementation milestones

1. **Phase 1 — Manual model contract:** Define the smallest immutable page/section representation for sourced metadata, ordered optional sections, declarations/overloads, and generic fallback. Decide how unknown sections are preserved. No database change at this stage.
2. **Phase 2 — Fixture and extraction slice:** Add representative saved fixtures for class/container (`vector`), member function (`push_back`), algorithm (`sort`), string (`string`), and distinct type (`optional`). Implement conservative deterministic projection against the actual `Document` blocks. Verify every displayed declaration/fact against fixture text; avoid unsupported fields.
3. **Phase 3 — Renderer:** Add manual layout to Rich using optional sections and stable ordering. Verify empty sections, unknown types, width, piped/no-color rendering, and overload distinction. Keep generic document rendering available.
4. **Phase 4 — CLI integration:** Route topic display through projection while keeping database lookup and all read commands offline. Preserve current generic/detail data and search/update behavior.
5. **Phase 5 — Accuracy/coverage:** Expand only after tests show dependable extraction. Add parser improvements where fixture evidence identifies information loss. Consider stored projections, detail options, or standard filters only as separate approved work with a concrete need.

## 10. Testing strategy

### Semantic extraction and projection

- Classify the representative class/container, member function, algorithm, string, and optional/type fixtures; verify unknown/ambiguous identity remains unknown.
- Assert exact signatures and separate overloads, including their ordering and any source overload labels that are preserved.
- Verify parameter type/name/description, return type/behavior, complexity, requirements, exceptions, standards, and invalidation only when fixture-backed. Include missing and ambiguous examples and assert the unsupported field is omitted.
- Check scope: a class-level note must not be attached to every member, and an overload-specific fact must not leak to another overload.
- Verify source URL/header attribution is retained and unclassified generic blocks remain available to detailed rendering.

### Dynamic rendering

- Render different section subsets for class, algorithm, function, and type pages; ensure no empty heading appears.
- Exercise known section ordering, preserved declaration order, an unknown section type, and safe fallback behavior.
- Test terminal width, long signatures, tables/member lists, piped output, color-disabled output, and pager behavior using the existing Rich test patterns.

### Regression and integration

- Keep existing document, parser, SQLite round-trip, search, CLI, update failure-preservation, package, and release tests passing.
- Add an end-to-end fixture-to-document-to-projection-to-render test for each representative entity shape.
- Keep the no-HTTP page/search test; add a read-path assertion that parsing is not invoked. Verify updates still parse and store generic documents and failures preserve the previous valid page.
- If the generic document schema changes (not recommended initially), test old database compatibility/migration and round-trip both generic and semantic representations.

## 11. Verification record

### Verified from source and repository

- Generic document pipeline and exact source paths summarized above.
- Catalog contains `vector`, `string`, `sort`, and `vector::push_back`; no `optional` entry.
- Only vector and push_back HTML fixtures exist in `tests/fixtures/`.
- The push_back parser test expects both overload declarations separately.
- Renderer prints surviving blocks in order; no semantic/manual projection is present.
- SQLite stores serialized generic blocks and searchable text, supports exact topic lookup and bounded search, and has no explicit schema-version migration mechanism.
- Existing test suite, run with `.venv\Scripts\python.exe -m pytest`: **34 passed** (Python 3.15.2, pytest 9.2.0).
- Repository-local `data/` has no files; no cached page corpus was available for the five-topic output comparison.

### Assumptions/recommendations, not current behavior

- Read-time projection is the safest first integration because one page is already loaded and duplicate semantic storage/schema migration can be avoided. Its cost must be measured if it becomes material.
- `std::optional` should be added only with explicit scope approval and a fixture/catalog decision; it is not currently downloadable.
- The proposed module name `core/manual.py` is illustrative. Choose a name after Phase 1 confirms the model boundary.

## Files inspected

- Project guidance and design: `AGENTS.md`, `docs/MANUAL_REFERENCE_DESIGN.md`, `docs/continuation.md`, `docs/roadmap.md`.
- User/developer/architecture docs: `README.md`, `docs/architecture.md`, `docs/developer-guide.md`, `docs/project-structure.md`, `docs/user-manual.md`, `docs/mvp.md`, `docs/technology-stack.md`.
- Application source: `src/cxxman/core/document.py`, `src/cxxman/core/search.py`, `src/cxxman/core/catalog.py`, `src/cxxman/core/update.py`, `src/cxxman/parser/html_parser.py`, `src/cxxman/storage/database.py`, `src/cxxman/ui/renderer.py`, `src/cxxman/cli.py`, `src/cxxman/__main__.py`, `src/cxxman/__init__.py`.
- Tests and fixtures: all `tests/test_*.py` files and both files under `tests/fixtures/`.
- Packaging/configuration context: `pyproject.toml`; repository directories including `data/`.
