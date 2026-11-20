AGENTS.md
Project instructions for Codex and other coding agents working in
cxxman.

These instructions apply to the entire repository unless a more specific
AGENTS.md overrides them.

1. Project mission
   Build cxxman, a cross-platform, terminal-based C++ documentation
   reference inspired by Unix/Linux manual pages.

The intended experience is:
cxxman vector
cxxman string
cxxman sort
cxxman search unordered_map
CXXMAN should help a programmer quickly understand and use a C++ entity
without having to read an entire cppreference article.

The default page should answer practical questions:

- What is this entity?
- Which header provides it?
- What is its declaration or signature?
- What are its parameters and return behavior?
- What operations or members are available?
- What complexity and behavioral guarantees matter?
- What important requirements, exceptions, or invalidation rules
  apply?
- Where can the full reference be found?
  The default output must be readable directly in the terminal. Do not
  open a browser or GUI when displaying a page.

Current scope: C++ only. Do not add C language documentation, POSIX
APIs, Linux system calls, or OS-specific APIs to the primary reference
scope unless explicitly requested.

The project is developed on Windows using PowerShell and Python 3.15.2.
Keep the implementation portable where practical. 2. Product principles

1. Local-first: ordinary page viewing and search use local data
   only.
2. Fast read path: opening a cached page must not perform HTTP
   requests or parse source HTML.
3. Concise, not incomplete: prioritize information needed for
   practical use without discarding important technical guarantees.
4. Manual-page first: present information in a recognizable Unix
   man-style layout, not as a flattened cppreference article.
5. Entity-aware: functions, classes, algorithms, aliases, concepts,
   and other entities need different applicable sections.
6. Dynamic sections: render only sections that exist and apply to
   the current entity.
7. Source-backed accuracy: never invent signatures, return types,
   complexity, exception guarantees, standard availability, or other
   technical facts.
8. Preserve detail: concise default output must not require
   throwing away useful extracted source information.
9. Small dependencies: prefer the Python standard library when it
   is sufficient.
10. Incremental delivery: implement one coherent milestone at a
    time.
11. Testable boundaries: parsing, semantic extraction, projection,
    storage, search, CLI, and rendering must be independently testable.
12. No speculative complexity: do not add abstractions,
    dependencies, background services, or features without a concrete
    need.
13. Product direction: from article viewer to manual reference
    The current system parses cppreference pages into a generic structured
    document and renders those document blocks.

That generic representation is useful as an ingestion result, but
rendering every parsed block in source order makes the output resemble
the original article.

The product direction is to add a semantic manual-page
transformation between the existing parsed document and the terminal
renderer.

The target conceptual pipeline is:
cppreference HTML
|
v
Existing HTML parser
|
v
Existing generic document model
|
v
Semantic extraction / classification
|
v
Manual-page projection
|
v
Semantic manual-page model
|
v
Terminal renderer
|
v
Concise C++ manual page
This is a target architecture, not permission to create all these
modules immediately. Inspect the actual code and choose the smallest
integration that fits the existing design.

Do not rewrite the parser, database, cache, search engine, or CLI just
to match this diagram. Reuse existing components where appropriate. 4. Local-first architecture
There are two separate execution paths.
Read path: offline
User command
-> CLI
-> local lookup
-> SQLite / local document storage
-> structured document model
-> manual-page projection (if needed)
-> terminal renderer
-> terminal
For commands such as cxxman vector and cxxman search vector:

- Never make an HTTP request.
- Never download source HTML.
- Never parse HTML.
- Read only the requested page or a bounded set of search results.
- Do not load the entire documentation corpus into memory.
- Do not invoke an LLM or remote service to summarize or construct a
  page.
  If manual-page projection is expensive, consider performing it during
  ingestion and storing the result. Make this decision based on the
  existing data model and measured cost; do not add duplicate storage
  without a clear benefit.
  Ingestion/update path: network allowed
  Explicit update command
  -> HTTP client
  -> downloaded HTML
  -> HTML parser
  -> generic structured document
  -> semantic extraction / manual-page projection
  -> local storage and search index
  Network access is allowed only for explicit commands: `cxxman update`
  fetches C++ reference pages, `cxxman check-updates` checks the GitHub
  application release, and `cxxman self-update` installs a newer standalone
  build. These app-update commands do not belong to the page-view or search
  read path.

Parse HTML only during ingestion or update. Persist the parsed
representation so the read path does not need the original HTML.

A failed update must not destroy a previously valid local page. Use
transactions or a staging-and-commit strategy. 5. Manual-page information architecture
5.1 General page layout
A page should follow the conventions of Unix manual pages. Depending on
the entity, applicable sections may include:

- NAME
- LIBRARY
- SYNOPSIS
- DESCRIPTION
- TEMPLATE PARAMETERS
- MEMBER TYPES
- MEMBER FUNCTIONS
- PARAMETERS
- RETURN VALUE
- COMPLEXITY
- REQUIREMENTS
- EXCEPTIONS
- INVALIDATION
- NOTES
- EXAMPLES
- STANDARDS
- SEE ALSO
- REFERENCES
  This list is not a mandatory template. It is a vocabulary of possible
  sections.
  5.2 Entity-aware sections
  Different entity kinds need different information.

Examples:

Class/container such as std::vector: - NAME - LIBRARY -
SYNOPSIS - DESCRIPTION - TEMPLATE PARAMETERS - MEMBER TYPES -
MEMBER FUNCTIONS - ITERATORS - CAPACITY - ELEMENT ACCESS -
MODIFIERS - COMPLEXITY - INVALIDATION - STANDARDS - SEE ALSO

Algorithm/function such as std::sort: - NAME - LIBRARY -
SYNOPSIS - DESCRIPTION - PARAMETERS - RETURN VALUE -
REQUIREMENTS - COMPLEXITY - EXCEPTIONS - STANDARDS - SEE ALSO

Type alias/concept/enum: - Use only sections supported by the
source, such as SYNOPSIS, DESCRIPTION, TEMPLATE PARAMETERS,
VALUE, STANDARDS, and SEE ALSO.

These are examples, not hardcoded page templates. The actual sections
must be determined from reliable source information and the entity's
semantics.
5.3 Dynamic section behavior

- Do not render empty headings.
- Do not force every entity to use the same sections.
- Do not create section content from assumptions.
- Preserve meaningful source ordering where it matters.
- Use consistent display ordering for known manual sections where
  appropriate.
- Support new section types without changing unrelated entity
  definitions.
- Unknown section types must be handled safely and must not crash the
  whole page.
  5.4 Concision rules
  The default page should prioritize: 1. Header and declaration. 2. Short
  purpose/behavior description. 3. Parameters and return behavior where
  applicable. 4. Common operations for classes and containers. 5.
  Important complexity and behavioral guarantees. 6. Important
  constraints, exceptions, and invalidation rules. 7. Standards and
  related references.

Avoid: - Navigation and page chrome. - Repeated descriptions. - Long
tutorials. - Unrelated article sections. - Exhaustive historical
discussion. - Reproducing every source paragraph without selection.

Do not remove information merely because it is long if it describes a
critical precondition, guarantee, exception, or behavioral distinction. 6. Semantic model and transformation
The existing generic document model should remain useful as the parsed
source representation.

A manual page is a different, more focused view of that information.
Keep the distinction clear.

The semantic/manual representation should support, as appropriate:

- Entity identity and display name.
- Entity kind (class, function, algorithm, alias, concept, enum,
  etc.).
- Language (cpp).
- Header and namespace.
- C++ standard availability.
- Source URL and source attribution.
- Ordered, typed, optional sections.
- Declarations and overloads.
- Structured parameters and return information.
- Structured operation/member summaries.
- Complexity and other guarantees where sourced.
  A conceptual example:
  ManualPage(
  name="vector",
  entity_kind="class",
  language="cpp",
  header="vector",
  sections=[
  NameSection(...),
  LibrarySection(...),
  SynopsisSection(...),
  DescriptionSection(...),
  TemplateParametersSection(...),
  MemberFunctionsSection(...),
  ComplexitySection(...),
  SeeAlsoSection(...),
  ],
  )
  This is illustrative pseudocode. Do not copy these exact class names or
  introduce a large class hierarchy without inspecting the existing code
  and demonstrating why it is needed.

Prefer lightweight typed structures and existing project conventions.
6.1 Separation of responsibilities
Keep these responsibilities conceptually distinct:

- Parsing: convert source HTML into generic structured content.
- Semantic extraction: identify facts and structures in parsed
  content.
- Projection: select and organize relevant facts into a concise
  manual page.
- Rendering: present the manual page in the terminal.
  Do not put cppreference-specific HTML selectors in the renderer.

Do not put Rich formatting in the parser or semantic extraction layer.

Do not make the CLI responsible for domain transformation.
6.2 Deterministic transformation
Semantic extraction and manual-page projection should be deterministic
and reproducible.

Do not use an LLM, network service, or nondeterministic summarizer at
runtime.

If source information cannot be reliably classified, preserve it in the
generic/detail representation rather than generating an inaccurate
manual section. 7. Source accuracy and C++ semantics
CXXMAN focuses on the C++ Standard Library and C++ language reference
material available through its supported source.

Distinguish standard guarantees from implementation-specific behavior.

- Treat ISO C++ standard semantics as the primary scope.
- Identify compiler-specific extensions explicitly if they are ever
  supported.
- Do not present GCC, Clang, MSVC, or operating-system behavior as
  universal C++ behavior.
- Do not mix POSIX or Linux man-page semantics into standard C++
  library entries.
  The source currently derives documentation from cppreference. Preserve
  source URLs and attribution so users can consult the original reference.
  7.1 Overloads and declarations
- Do not merge materially different overloads into one misleading
  signature.
- Preserve overload distinctions when they affect parameters, return
  types, constraints, or behavior.
- Preserve template parameters and relevant constraints.
- Do not simplify a declaration in a way that changes its meaning.
- If only some overloads can be extracted reliably, show only those
  and make the limitation clear.
  7.2 Guarantees and behavioral details
  Never infer or fabricate:
- Return types or return values.
- Complexity guarantees.
- Exception guarantees.
- Preconditions or requirements.
- Iterator/reference invalidation rules.
- Thread-safety guarantees.
- Standard introduction, deprecation, or removal versions.
- Feature-test macros.
- Portability claims.
  Absence of information is not evidence of a guarantee.

If a fact is unavailable or extraction is ambiguous, omit it from the
concise page or identify it as unavailable. Do not silently guess.
7.3 C++ standard versions
Where the source provides reliable information, preserve availability
and change history, including relevant versions such as C++11, C++14,
C++17, C++20, and C++23.

Do not assume an entity is available in every standard version.

Standard-version filtering is a possible future feature, not a
prerequisite for the initial manual-page transformation. 8. Section system and rendering
Use a modular section-rendering mechanism or an equivalent design that
fits the current codebase.

A conceptual registry might look like:
SECTION_RENDERERS = {
"name": render_name,
"library": render_library,
"synopsis": render_synopsis,
"description": render_description,
"parameters": render_parameters,
"return_value": render_return_value,
"complexity": render_complexity,
"exceptions": render_exceptions,
"examples": render_examples,
"see_also": render_see_also,
}
This is an example, not a mandatory implementation.

Requirements:

- Render only sections present in the manual page.
- Keep section formatting consistent.
- Handle unknown section types safely.
- Keep content data separate from terminal styling.
- Avoid one large conditional renderer that embeds all entity-specific
  extraction rules.
- Do not create an abstraction for every section if simple data-driven
  rendering is sufficient.
  The renderer should support terminal width, readable indentation,
  code/signature formatting, and piped output without requiring color.

9. Detail and source access
   The concise manual page is the default experience.

The project should preserve enough source/document data to support a
more detailed view or direct reference to cppreference.

A future command may provide a detailed mode, for example:
cxxman vector --detail
Do not implement this option unless it fits an approved milestone or the
feature already exists.

Do not discard generic parsed content just because it is not included in
the default manual page.

Every manual page should retain a source reference when available. 10. Performance requirements
Treat these as architectural requirements:

- No network access on page-view or search commands.
- No HTML parsing on page-view or search commands.
- Use SQLite indexes for exact symbol lookup and search.
- Evaluate SQLite FTS5 for full-text search; check whether the active
  Python SQLite build supports it before depending on it.
- Load one page at a time.
- Bound search result counts.
- Avoid scanning the full corpus in Python for each query.
- Avoid expensive work during CLI startup.
- Use pagination for long terminal output.
- Do not introduce caching layers on top of the local database unless
  measurements justify them.
  Measure rather than guess. Use timeit for focused operations and
  cProfile for profiling. Record corpus size and machine details when
  reporting measurements. Do not claim performance targets have been met
  without measurements.

11. Technology stack
    Use the following planned stack unless a compatibility issue or measured
    reason justifies a change:
    Area Technology Rules
    Language Python 3.15.2 Verify the actual
    development interpreter interpreter used by the
    project venv

CLI argparse Standard library; do
not add Typer for the
MVP

HTML parsing BeautifulSoup4 Used only during
ingestion/update

HTTP HTTPX Used only by explicit
network commands

Storage SQLite via sqlite3 Prefer standard-library
SQLite

Terminal UI Rich Rendering only; no
networking or
persistence

Tests Pytest Unit and integration
tests
Profiling timeit, cProfile Standard-library tools
Dependency compatibility
Before adding or upgrading third-party packages:

1. Check that the package supports the active Python version.
2. Prefer a compatible wheel over an unnecessary native build.
3. Install into the project virtual environment, never globally.
4. If a required dependency is incompatible, stop and report the issue
   with options. Do not silently downgrade Python or replace the agreed
   architecture.
   Keep runtime dependencies minimal. Do not add a package merely because
   it is familiar.
5. Project structure
   Use the existing repository structure as the source of truth. The
   following is a conceptual direction, not a mandate to create every
   module:
   cxxman/
   ├── AGENTS.md
   ├── pyproject.toml
   ├── README.md
   ├── docs/
   ├── src/
   │ └── cxxman/
   │ ├── cli.py
   │ ├── core/
   │ │ ├── document.py
   │ │ └── search.py
   │ ├── parser/
   │ │ └── html_parser.py
   │ ├── storage/
   │ │ └── database.py
   │ └── ui/
   │ └── renderer.py
   ├── tests/
   └── data/
   New semantic/manual modules should be added only when a real
   responsibility requires them. Possible names include manual.py,
   projection.py, or sections/, but inspect the repository before
   choosing.

Keep downloaded documentation and generated databases outside src/. Do
not commit .venv/, local documentation databases, or downloaded corpus
data. 13. Existing module responsibilities
Follow existing boundaries unless repository inspection demonstrates a
concrete reason to change them.

- cli.py: parse arguments, dispatch commands, and translate expected
  errors into useful terminal messages. Keep domain logic out of the
  CLI.
- core/document.py: define generic parsed-document and content-block
  structures.
- core/search.py: implement search behavior over the local index.
- parser/html_parser.py: convert source HTML into generic structured
  document objects. It must not render to the terminal or write
  arbitrary database queries.
- storage/database.py: own SQLite schema, connection handling,
  transactions, and persistence operations.
- ui/renderer.py: render prepared page/document objects using Rich.
  It must not fetch network content or query the database directly.
- **main**.py: remain a minimal module entry point.
  If a semantic extraction or manual-page projection component is
  introduced, give it a focused responsibility and test it independently.

Use clear interfaces between modules. Avoid circular imports. 14. HTML parser requirements

- Build parser tests from saved HTML fixtures; parser tests must not
  require internet access.
- Parse semantic HTML elements and preserve their order.
- Preserve code blocks, headings, paragraphs, lists, and tables as
  distinct structures.
- Strip navigation, menus, sidebars, footers, and unrelated page
  chrome where possible.
- Do not assume every cppreference page has identical HTML.
- Handle missing or changed elements gracefully.
- Do not use regex as the primary HTML parser.
- Keep source-specific HTML handling inside the parser module.
- Record unsupported structures as known limitations rather than
  silently corrupting the whole page.
  The semantic/manual transformation must consume the parsed document
  representation where practical. Avoid duplicating HTML parsing logic in
  a second component.

15. Storage and search requirements

- Use SQLite through Python's sqlite3 module.
- Define schema creation/migration behavior explicitly.
- Use parameterized SQL for all values.
- Use transactions for writes and updates.
- Add indexes for canonical topic and other frequently queried fields.
- Consider FTS5 for full-text search only after verifying support.
- Keep search results bounded and deterministic.
- Do not fetch or parse HTML during search.
- Avoid loading all page bodies for a metadata-only search.
  If storing a semantic manual-page representation, decide whether it
  should be stored alongside the generic document or derived from it.
  Document the trade-off and avoid redundant copies unless they provide a
  measurable or operational benefit.

Any schema change must include an explicit migration/compatibility plan
for existing local databases. 16. Terminal rendering requirements
Use Rich for terminal presentation.

The manual-page renderer should:

- Follow a consistent Unix man-inspired layout.
- Respect terminal width.
- Render section headings clearly.
- Use readable indentation and spacing.
- Present signatures and code in distinct blocks.
- Keep parameter/member lists scannable.
- Preserve table relationships when a table is genuinely the best
  representation.
- Avoid reproducing article-style walls of text.
- Support pagination for long output.
- Degrade gracefully when output is piped or terminal color is
  unavailable.
  Do not use color as a substitute for structure. Readability comes first.

17. CLI behavior
    Use argparse.

Existing/planned commands include:

- cxxman --help
- cxxman --version
- cxxman <topic>: display a locally stored manual page.
- cxxman search <query>: search the local index.
- cxxman list: list locally available pages.
- cxxman update: explicit network-enabled ingestion/update
  operation.
- cxxman check-updates: explicitly check whether a newer application
  release exists.
- cxxman self-update: explicitly download and install a verified release
  for a standalone installation; Python-managed installations use pip or
  their source checkout.

Do not break existing command syntax without an approved reason.

Potential future options include:

- --detail: show more of the preserved source documentation.
- --std: filter or describe availability for a selected C++
  standard.
- --source: show or open the source reference, if supported by the
  existing CLI design.
  These are not automatic implementation requirements.

If a page is not available locally, report that clearly and tell the
user which explicit update command can obtain it. Do not silently fall
back to the network.

Return sensible exit codes. Keep output useful and concise. 18. Coding conventions

- Use Python type hints for public functions and important data
  structures.
- Follow PEP 8.
- Use descriptive names; avoid clever abbreviations.
- Keep functions focused and reasonably small.
- Prefer explicit control flow over dense one-liners.
- Use pathlib.Path for filesystem paths.
- Use context managers for files, database connections/cursors, and
  HTTP resources.
- Use parameterized SQL.
- Catch specific exceptions. Do not hide errors with broad
  except Exception unless re-raising or handling at a true
  application boundary.
- Do not use global mutable state for application services.
- Do not add comments that merely repeat the code.
- Add docstrings to public modules/classes/functions where they
  clarify contracts.
- Keep Windows paths and shell commands correct for PowerShell; avoid
  hard-coded user-specific paths.
- Do not modify unrelated files or reformat the entire project without
  need.

19. Testing requirements
    Use Pytest.

Tests should be deterministic and must not rely on live network access
unless explicitly marked as a separate integration test.

Minimum expectations:
Existing behavior

- Unit tests for generic document model behavior.
- Parser tests using local HTML fixtures.
- Storage tests using temporary SQLite databases.
- Search tests for exact matches, partial matches, no results, and
  bounded results.
- CLI tests for help, version, missing pages, search, and exit codes.
- A test proving that page viewing and search do not make HTTP
  requests.
- Update tests for network failures and preservation of the last valid
  local version.
  Manual-page behavior
- Tests for entity classification where applicable.
- Tests for semantic extraction from representative parsed documents.
- Tests for dynamic section inclusion and omission.
- Tests proving that empty sections are not rendered.
- Tests for unknown section types.
- Tests for overload preservation.
- Tests for missing or ambiguous source information.
- Tests for manual-page rendering and terminal-width behavior.
- Accuracy tests for signatures, return types, complexity, and
  standard availability when those facts are represented.
  Use saved fixtures based on representative cppreference pages. Include
  more than one entity shape, at minimum a class/container, an
  algorithm/function, and another distinct entity type.

Do not test only that output contains text. Test that extracted
technical information is correct and that unsupported information is not
fabricated.

Run relevant tests after each change, then run the full test suite
before completing a milestone. Do not claim tests passed unless they
were actually run. 20. Implementation roadmap
Work incrementally. Do not implement every phase in one large change.
Phase 0: Repository audit
Before modifying code:

1. Inspect the complete repository and current implementation.
2. Read README.md and relevant files under docs/.
3. Inspect the current Git status and preserve user changes.
4. Trace the current path from downloaded HTML to terminal output.
5. Inspect the generic document model, parser, renderer, storage,
   cache, search, and tests.
6. Inspect representative cached or fixture pages for different C++
   entity types.
7. Identify the smallest safe integration point for semantic manual
   pages.
   Deliverable: a concise audit and implementation plan with actual
   repository file paths. Do not modify code during this phase.
   Phase 1: Manual-page model
   Design the smallest typed representation that can express:

- Page metadata.
- Entity kind.
- Source attribution.
- Optional ordered sections.
- Structured content needed by the first supported entities.
  Do not build a speculative universal documentation framework.
  Phase 2: Semantic extraction and projection
  Implement extraction/projection for a small representative set:

1. std::vector
2. std::string
3. std::sort
4. std::unique
5. std::optional
   These represent different documentation structures. Verify extraction
   quality before expanding coverage.
   Phase 3: Manual renderer
   Implement the concise Unix-style terminal presentation.

Reuse Rich and existing renderer utilities where appropriate. Keep
extraction rules out of the renderer.
Phase 4: Integration
Integrate manual pages into the existing CLI while preserving:

- Existing search functionality.
- Existing caching.
- Existing offline access.
- Existing update mechanisms.
- Existing cross-platform behavior.
  Phase 5: Validation and coverage
  Compare generated manual pages against the source documentation.

Validate declarations, overloads, parameters, return behavior,
complexity, exceptions, standard availability, and related references.

Expand coverage only after extraction is reliable. 21. Required agent workflow
Work in small, reviewable milestones.
Before changing code

1. Inspect the existing repository and read README.md and relevant
   files under docs/.
2. Check the current Git status. Do not overwrite user changes.
3. Read this AGENTS.md and any more specific agent instructions.
4. Identify the smallest coherent implementation step.
5. State the files you intend to create or modify and why.
6. For a new architectural direction, explain how it fits the existing
   implementation.
   While working
7. Implement only the agreed milestone.
8. Add or update tests with the implementation.
9. Do not create placeholder modules without a current use.
10. Do not add unrelated features.
11. Do not change the technology stack or architecture silently.
12. Do not make network calls from read-path code.
13. Do not discard existing generic document content just because the
    concise manual page does not display it.
14. Do not fabricate semantic information when extraction is incomplete.
15. Prefer incremental compatibility-preserving changes over broad
    rewrites.
    Before finishing
16. Run relevant tests.
17. Run the full test suite if practical.
18. Run formatting/lint tools only if configured; do not add them
    without a reason.
19. Inspect git diff and git status.
20. Summarize implementation, tests run, results, and known limitations.
21. Identify the next smallest milestone, but do not start it without
    user approval.
    If a requirement is ambiguous or a dependency is incompatible with
    Python 3.15.2, stop and explain the trade-off rather than guessing.
22. Current project state
    The project is a public C++ terminal reference. Release v0.2.0 is
    published with native Windows x64, Linux x64, Intel macOS, and Apple
    Silicon builds. It includes a 50-page curated catalog, deterministic
    manual-page projection, Rich C++ syntax highlighting, `--color
    auto|always|never`, `check-updates`, `self-update`, and a Windows
    per-user setup executable with a generated uninstaller. The Windows
    installer and uninstaller passed CI install/remove coverage. The full
    local suite passes 57 tests. Release v0.1.0 is historical; v0.2.0 is the
    current release. The reference remains a curated selection, not complete
    cppreference coverage.

The repository is an existing working project. Do not repeat the
original skeleton task or assume that the proposed architecture has
already been implemented.

For future work:

1. Read docs/continuation.md, docs/roadmap.md, and relevant
   user/developer guides before choosing a milestone.
2. Inspect Git status and the current implementation.
3. Verify which features are actually implemented rather than relying
   only on this file.
4. Keep each change focused, tested, and committed locally only when
   requested by the user.
5. Continue improving concise, entity-aware C++ manual pages without
   claiming coverage beyond the curated catalog and tested extraction.
6. The Phase 0 repository audit is recorded in
   `docs/manual-reference-audit.md`; refresh its findings if the
   architecture changes materially rather than repeating the original
   audit.
7. std::vector::push_back has previously been implemented; verify its
   current state and tests before modifying it.
   Final principle
   CXXMAN should behave like a local C++ manual reference, not a
   terminal copy of cppreference.

Optimize the default experience for fast, accurate answers:

- What is it?
- Where is it declared?
- How do I use it?
- What does it return?
- What guarantees does it provide?
- What should I watch out for?
  Keep the comprehensive source available when the user needs more depth.

Accuracy first. Concision second. Never trade correctness for a
shorter page.
