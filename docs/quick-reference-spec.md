# Quick-reference page and full-library coverage spec

Status: approved product direction (2026-11-20). M1 real fixtures completed
on 2026-11-21; M4–M8 completed on 2026-11-22.
Companion to `docs/audit-2026-11-manual-quality.md` (defects) — this file
defines **what the default page must look like** and **how coverage grows
to the whole C++ standard library**.

## 1. Product goal (owner's words, summarized)

The terminal output must be short. A developer should see immediately:

- what the entity is for (which one to use for the task),
- how to include it,
- its signature(s), including the **return type**,
- how to use it (a short example),
- and only the pitfalls that matter.

Long article text belongs behind `--full` or on cppreference. Coverage
must extend from the 50-page curated catalog to the **entire C++
standard library** documented by cppreference.

Accuracy rules in `AGENTS.md` sections 7 and 7.2 still apply: brevity is
achieved by **selecting and hiding**, never by rewording, summarizing, or
inventing. Every line shown comes from the source.

## 2. Default page: the "quick card"

### 2.1 Budget

- Target: fits one screen — about 30 lines at 100 columns for a function,
  about 40 for a class. This is a soft budget enforced by section
  selection rules below, not by truncating sentences.
- No pager when the card fits the terminal height.
- When content is hidden, say so in one dim line, e.g.
  `+3 overloads with ExecutionPolicy (C++17) — cxxman sort --full`.
  Hidden content is never silently dropped.

### 2.2 Sections and selection rules (functions / algorithms)

| Section | Content | Rule |
|---|---|---|
| NAME | `std::sort — sorts a range into ascending order` | Purpose text = first sentence of the source description (`t-li1` or first paragraph). If none, show the name only. Never synthesize. |
| HEADER | `#include <algorithm>` | From the `Defined in header` row (fallback: catalog header). |
| SYNOPSIS | Numbered signatures with `(since C++NN)` / `(until C++NN)` / `(constexpr since …)` tags | Show all non-ExecutionPolicy overloads; collapse ExecutionPolicy and `std::ranges::` variants into one counted hint line. Never merge distinct overloads into one. Return type is visible because the full declaration is shown. |
| PARAMETERS | `first, last — the range of elements to sort` one line per parameter | From the source parameter table. Type-requirement rows go to `--full` with a hint line. |
| RETURNS | Source return-value text | Show whole section if ≤ 3 lines wrapped; otherwise first paragraph + hint. `(none)` only if the source says so. |
| COMPLEXITY | Source text | Same rule as RETURNS. |
| WATCH OUT | Up to 3 items, verbatim source sentences | Taken only from INVALIDATION, EXCEPTIONS, and explicit "behavior is undefined" / precondition sentences. If more exist, hint line. If none in source, omit the section (absence is not a guarantee). |
| EXAMPLE | Source example | Show only if ≤ 15 code lines; otherwise one hint line `cxxman sort --example`. Never edit or shorten example code. Hide "Output:" block in the card. |
| SEE ALSO | Names only, one or two lines, comma-separated | From the source See-also table. |
| footer | `More: cxxman sort --full · https://en.cppreference.com/cpp/algorithm/sort` | Always present. |

### 2.3 Classes / containers

NAME, HEADER, SYNOPSIS (template declaration with tags), one-paragraph
DESCRIPTION (first source paragraph only), MEMBERS (grouped, names only,
comma-separated per source group), NON-MEMBERS (names only), WATCH OUT
(iterator invalidation first), EXAMPLE (same rule), SEE ALSO, footer.

Member detail is reached with `cxxman vector::insert` (member pages are
their own entries; see section 4).

### 2.4 Other kinds

Aliases, type traits, concepts, constants, enums: NAME, HEADER, SYNOPSIS,
DESCRIPTION (first paragraph), VALUE / MEMBER CONSTANTS when sourced,
EXAMPLE rule, SEE ALSO, footer. Unknown kinds fall back to NAME, HEADER,
SYNOPSIS, first paragraph, footer.

### 2.5 Illustrative layouts

Layout only. Every value must come from the stored source document;
these mockups are not test oracles.

```
std::sort — sorts a range into ascending order
#include <algorithm>

SYNOPSIS
  (1) template< class RandomIt >
      void sort( RandomIt first, RandomIt last );          (constexpr since C++20)
  (2) template< class RandomIt, class Compare >
      void sort( RandomIt first, RandomIt last, Compare comp );
  +2 overloads with ExecutionPolicy (C++17), std::ranges::sort (C++20) — --full

PARAMETERS
  first, last   the range of elements to sort
  comp          comparison function object, returns true if a < b

RETURNS     (none)
COMPLEXITY  O(N·log(N)) comparisons, where N = std::distance(first, last)  (since C++11)

WATCH OUT
  • The order of equal elements is not guaranteed to be preserved.

EXAMPLE
  std::array<int, 10> s{5, 7, 4, 2, 8, 6, 1, 9, 0, 3};
  std::sort(s.begin(), s.end());
  ...

SEE ALSO  stable_sort, partial_sort, ranges::sort, nth_element
More: cxxman sort --full · https://en.cppreference.com/cpp/algorithm/sort
```

```
std::vector — dynamic contiguous array
#include <vector>

SYNOPSIS
  (1) template< class T, class Allocator = std::allocator<T> > class vector;
  (2) namespace pmr { template< class T > using vector = ...; }   (since C++17)

MEMBERS
  Access      at, operator[], front, back, data
  Iterators   begin/cbegin, end/cend, rbegin/crbegin, rend/crend
  Capacity    empty, size, max_size, reserve, capacity, shrink_to_fit
  Modifiers   clear, insert, emplace, erase, push_back, emplace_back,
              pop_back, resize, swap, insert_range (C++23), append_range (C++23)
NON-MEMBERS   operator==, operator<=>, swap, erase, erase_if

WATCH OUT
  • Iterator invalidation: see `cxxman vector --full` (table: 6 rows)

SEE ALSO  array, deque, inplace_vector (C++26)
More: cxxman vector::push_back · cxxman vector --full · <source url>
```

## 3. Discovery UX ("what should I use?")

1. **Header index pages**: `cxxman algorithm`, `cxxman <vector>`,
   `cxxman header algorithm` list every entity in the header with its
   one-line purpose, grouped as the source groups them (e.g.
   "Sorting operations", "Binary search"). Source: cppreference
   `/cpp/header/<name>` pages (`t-dsc` rows). A topic name that is both a
   header and an entity (`vector`, `string`) opens the entity; the header
   listing is `cxxman header vector`.
2. **Task search**: `cxxman search remove duplicates` should rank
   `unique` near the top. Store each page's one-line purpose in its own
   column and FTS field; rank exact topic / alias match > name prefix >
   purpose match > body match. Show the purpose in results:
   `unique   <algorithm>   removes consecutive duplicate elements in a range`.
3. **Aliases**: one source page often documents several names
   (`all_of, any_of, none_of`; `count, count_if`; `find, find_if,
   find_if_not`). Every listed name is a lookup key for that page.
   `std::` prefix stays optional; `std::ranges::sort` must resolve to its
   own page when present.
4. **Not-found**: suggest up to 3 close matches across names and aliases,
   plus the header listing if the query is a header name.

## 4. CLI options (target)

| Command | Purpose |
|---|---|
| `cxxman <topic>` | quick card (default) |
| `cxxman <topic> --full` | full stored document (existing `render_document`) |
| `cxxman <topic> --example` | examples only, untruncated |
| `cxxman header <name>` | header index listing |
| `cxxman search <words>` | ranked search with purpose lines |
| `cxxman update` | obtain/refresh the whole library (see section 5) |
| `cxxman update <topic ...>` | refresh specific pages from source |

Existing commands and `--color` must keep working. Broken pipe
(`| head`, `| Select-Object -First`) must exit quietly.

## 5. Full-library coverage

### 5.1 Scope

Include every C++ standard library page under cppreference `/cpp/`
reachable from `/cpp/header` and `/cpp/symbol_index` (≈1,200 index
links, plus class member pages; expect a few thousand pages), including
C-compatibility headers documented under `/cpp/` (`<cstring>`,
`<cmath>`, …) as `std::` entities.

Exclude: `/c/` pages, `/cpp/language/*`, keyword pages, preprocessor,
compiler-support tables, `experimental`/TS pages, and pure navigation
or index pages (other than header pages used for section 3.1).

### 5.2 Generated catalog

Replace the hand-written `_PAGE_DATA` in `src/cxxman/core/catalog.py`
with a generated, checked-in data file (e.g. `src/cxxman/data/catalog.json`,
loaded lazily) produced by a maintainer tool `tools/build_catalog.py`.
Each entry: topic, aliases, source path, header, entity kind, purpose
line (from the header/index page), parent (for members). The tool is
network-enabled and maintainer-only; the read path only reads the file.
Keep the current 50 topics resolvable with the same names.

### 5.3 Distribution — decision required

Crawling thousands of pages on every user's machine is slow and
impolite to cppreference. Recommended:

- A CI job (or maintainer command) builds the full SQLite database from
  the generated catalog with a polite crawler (≥1 s between requests,
  retries with backoff, resumable, per-page commits, project URL in the
  User-Agent).
- The database is published as a release asset with a `.sha256` sidecar,
  versioned separately from the app (data release tag).
- `cxxman update` downloads and verifies that single database file and
  swaps it in atomically; `cxxman update <topic>` keeps the current
  per-page source path for targeted refreshes.

**Licensing**: cppreference content is CC-BY-SA 3.0 / GFDL.
Redistributing a derived database requires attribution and the same
license for that data. Add a NOTICE for the data asset and show the
attribution in `--full` and `--version` output.

The owner must approve 5.3 (prebuilt data distribution and its license
terms) before Codex implements it. Until then, implement 5.2 and keep
per-page `update` working at full scale (per-page commits, failure
report, rate limiting).

### 5.4 Validation at scale

A maintainer tool renders every stored page and reports: pages without
NAME purpose, without SYNOPSIS, with duplicated table rows, with
concatenated names, with cards over budget, and parse failures. Commit
the summary counts to `docs/coverage-report.md`. Do not claim coverage
of pages that fail these checks; list them.

## 6. Milestones (supersede the order in the audit file)

1. **M1 Real fixtures** — trimmed real cppreference HTML for: class
   (`vector`), algorithm with policy overloads (`sort`), multi-name page
   (`all_any_none_of`), member function (`vector/push_back`), class
   template utility (`optional`), type trait (`is_same`), concept
   (`same_as`), constant/object (`npos` or `cout`), header page
   (`header/algorithm`). Accuracy tests per audit P1-3.
2. **M2 Parser fidelity** — audit P0-2, P0-3, P0-4, P1-1, P1-5.
3. **M3 Quick-card projection and renderer** — sections 2.1–2.4, audit
   P0-1, P1-4, P1-6; card-budget tests; "hidden content" hint lines.
4. **M4 Storage format version** — audit P1-2; new columns (purpose,
   aliases, format version) with explicit migration.
5. **M5 CLI UX** — `--full`, `--example`, broken pipe, ranked search with
   purpose lines, aliases, better not-found (sections 3.2–3.4, 4).
6. **M6 Generated catalog + header index** — sections 3.1, 5.1, 5.2.
7. **M7 Scale ingestion** — resilient per-page crawler; prebuilt data
   distribution only after owner approval of 5.3.
8. **M8 Validation sweep** — section 5.4; fix the top failure classes.

One milestone per change; keep the suite green; update status here.

M1 status: complete (2026-11-21). `tools/download_real_fixtures.py`
explicitly fetches and trims the nine attributed cppreference pages under
`tests/fixtures/real/`.

M2 status: complete (2026-11-21). The parser now preserves `t-li*`
descriptions and declaration labels/availability, keeps nested revision
rows inside their source cells without duplicating table rows, formats
`.t-lines` names and revision marks, handles uneven tables, and filters
maintenance boxes and citation back-links. Declaration metadata is stored
in document format 2; databases with the previous schema are migrated by
adding `format_version` with value 1, and old JSON records remain readable.
The real-fixture parser tests pass; M3 resolved the remaining P0-1
projection failure and moved the remaining P1-5 display cleanup into NOTES
and names-only member lists.

M3 status: complete (2026-11-21). The default projection now produces a
source-backed quick card with purpose, `#include`, annotated signatures,
selected parameters/returns/complexity, up to three watch-outs, a short
example or an example hint, names-only member/related lists, hidden-detail
hints, and a source footer. Real `vector` invalidation rows are shown under
WATCH OUT. At 100 columns, tests enforce 30 lines for a function card and
42 for a class card (the class target is approximate). Empty sections are
omitted, and the renderer skips the pager when its card estimate fits the
terminal height. M4 and M5 are complete; catalog/header work remains M6.

M4 status: complete (2026-11-22). SQLite stores the source-derived
purpose and catalog-provided aliases alongside each page. The record
format is version 3; opening older databases adds the metadata columns
transactionally and preserves existing format versions, so those pages
remain readable and are identified for targeted refresh. Page updates
continue to write atomically. No aliases were inferred for catalog entries
that do not provide them.

M5 status: complete (2026-11-22). Topic pages accept `--full` and
`--example`; example mode selects the complete source example section.
Search returns bounded results ordered by exact name/alias, name prefix,
purpose, then body, and prints each result's header and stored purpose.
Aliases resolve for cached reads and explicit updates. Not-found output
offers up to three catalog name/alias suggestions and recognizes header
names. Broken pipes exit quietly. Existing FTS indexes are upgraded to
index purpose and aliases; no page HTML is fetched on the read path.

M6 status: complete (2026-11-22). `tools/build_catalog.py` generated the
checked-in, lazily loaded `src/cxxman/data/catalog.json` from cppreference
header and symbol indexes. The catalog contains 2,496 named entries across
111 headers, excluding experimental/TS routes and library overview pages,
and including member routes, source groups, purpose text, aliases, and parent links where the
source identifies a class member. The original
50 curated pages remain the no-argument `update` set; specific catalog names
and aliases can be updated individually. `cxxman header <name>`, `cxxman
<header>`, and `cxxman <header-in-angle-brackets>` list grouped source rows;
a matching entity name such as `vector` still opens the entity page. The
catalog retains source routes and attribution. Metadata whose source row
does not identify a kind or purpose remains unknown or empty. Header and
symbol index extraction is covered by a saved real cppreference header
fixture. The generated data is metadata only; prebuilt database distribution
remains deferred under the approval rule in section 5.3.

M7 status: complete (2026-11-22). `update` without topics now selects the
whole generated catalog. Each successful page is parsed and committed
individually; a failed page is reported while later pages continue. Requests
are separated by at least one second, use the project URL in `User-Agent`,
and retry network failures, HTTP 429, and server errors with exponential
backoff. `update --resume` skips pages already stored, allowing an interrupted
catalog run to continue without replacing every successful page. Targeted
topic updates remain available. Prebuilt database distribution was not
implemented; it still requires owner approval under section 5.3.

M8 status: complete (2026-11-22). `tools/validate_coverage.py` reads the
selected SQLite database in read-only mode, loads one stored page at a time,
projects and renders its quick card, and reports parse failures, missing
NAME purpose, missing SYNOPSIS, duplicate table rows, known concatenated
member-name patterns, stale stored formats, and card-budget overruns. The
latest `docs/coverage-report.md` covers all 2,167 unique routes in the
2,496-name catalog and reports zero page/category findings. Four pages do not
provide a source purpose sentence; their NAME lines show only the title. No
declaration is fabricated for pages whose source has none. The database has
no records outside the generated catalog. The sweep checks stored structure
and rendered card budgets, not every technical fact against live source.
