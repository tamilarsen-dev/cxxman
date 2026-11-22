# Manual-page quality audit (2026-11-20)

Scope: v0.2.0 on `master` (`401f961`). Audit only; no code was changed.
Method: read all of `src/`, ran the suite (57 passed), rendered the locally
stored `vector`, `array`, and `vector::push_back` pages with
`--color never` at 100 columns, and re-parsed a freshly downloaded
`https://en.cppreference.com/cpp/container/vector` with the current parser
for comparison.

## Verdict

The architecture is sound and matches `AGENTS.md`: offline read path,
parse-once ingestion, atomic batch saves, FTS5 with LIKE fallback, a
separate projection layer, and Rich kept in the renderer. Release and
installer work is solid.

The weak point is the **content of the manual page**. On real
cppreference pages the output still reads like a re-flowed article, and
several defects lose or corrupt information that the project principles
call critical (invalidation rules, standard availability, the one-line
"what is it" sentence). The test suite did not catch any of this because
every HTML fixture under `tests/fixtures/` is a hand-written synthetic
page under 1 KB that does not use real cppreference markup.

`std::vector::push_back` is the best page today and a good target shape
for other pages.

## Findings

Severity: **P0** = drops or corrupts a technical fact; **P1** = clearly
wrong or confusing output; **P2** = polish/usability.

### P0-1 Recognized sections are silently dropped

Resolved in M3: the concise class card maps `Iterator invalidation` to the
WATCH OUT section and maps non-member, specialization, helper, and deduction
guide headings explicitly. Content outside the quick-card selection stays
in the generic document for `--full`.

`src/cxxman/core/manual.py:31` `_SECTION_TITLES` matches whole headings
exactly. Real headings that are not in the map are discarded from the
manual page:

| Real heading (vector/array) | Result |
|---|---|
| `Iterator invalidation` | dropped (map only has `invalidation`) |
| `Non-member functions` | dropped |
| `Specializations` (e.g. `vector<bool>`) | dropped |
| `Helper classes`, `Deduction guides` | dropped |
| `Defect reports` | dropped (acceptable to keep out of default) |

So `cxxman vector` and `cxxman array` show **no INVALIDATION section**
even though the source has one. Fix: add the missing mappings
(`iterator invalidation` → INVALIDATION, `non-member functions` →
NON-MEMBER FUNCTIONS, `specializations` → SPECIALIZATIONS,
`helper classes` → HELPER CLASSES, `deduction guides` →
DEDUCTION GUIDES), and decide explicitly which headings are intentionally
excluded (keep that as a named set, not an implicit fall-through).
Add a test that asserts INVALIDATION is present for a real `vector`
fixture.

### P0-2 The "what is it" sentence is lost

Resolved in M2: `div.t-li*` content is now retained as paragraphs, including
the source overload number.

cppreference puts per-declaration descriptions in
`<div class="t-li1"><span class="t-li">1)</span> ... text ...</div>`.
`src/cxxman/parser/html_parser.py:115` recurses into `div` but only yields
text from `p`, `pre`, lists, and tables, so bare text inside a `div` is
discarded. For `vector`, the sentence "std::vector is a sequence
container that encapsulates dynamic size arrays." never reaches the
document. Fix: treat `div.t-li1` (and similar `t-li*`) as a paragraph,
keeping the `1)` marker so it can be linked to overload (1). Re-check
other pages for other text-bearing `div`s.

### P0-3 Declaration annotations are discarded

Resolved in M2: declarations preserve their overload label and availability
metadata in optional `CodeBlock` fields. SQLite stores the fields in
document format 2; opening an older database adds `format_version` with
value 1, and older JSON records remain readable.

`_parse_declarations` (`html_parser.py:137`) keeps only `cells[0]`. The
overload number column `(1)` and the revision column `(since C++17)`,
`(until C++20)`, `(constexpr since C++20)` are dropped, and the
`Defined in header <...>` row is ignored. This loses standard
availability (e.g. `std::pmr::vector` is C++17) and breaks
cross-references such as "in order to use overload (1)" in the
PARAMETERS section. Fix: extend the generic model minimally (for example
optional `label` and `availability` fields on a declaration code block,
or a small `Declaration` block type) and render them beside each
signature. Requires a storage serialization update; see P1-4.

### P0-4 Nested revision tables are duplicated

Resolved in M2: only rows owned by the current table are parsed. Nested
revision rows remain in their source cell on separate lines with version
marks, and uneven rows are padded safely.

`_parse_table` (`html_parser.py:120`) uses `find_all("tr")`, which is
recursive. cppreference nests `t-rev-begin` tables inside cells for
"(until C++11) / (since C++11)" variants. The outer cell is flattened
into one long string **and** each nested row is appended again as a row
of the outer table. Visible in `vector` TEMPLATE PARAMETERS and
MEMBER TYPES (each requirement printed twice, the second time as a broken
two-column row). Fix: only take `tr` rows belonging to this table
(`tbody > tr` / direct rows), and render nested revision rows inside
the cell as separate lines with their version tags.

### P1-1 Words concatenated in member tables

Resolved in M2: `.t-lines` names are joined with commas and revision marks
are separated from member names.

`_text` uses `get_text("")`. cppreference lists overload names as
sibling spans, producing `begincbegin(C++11)`, `endcend(C++11)`,
`assign_range(C++23)`. Fix: join `span` children of `.t-lines` with
`", "` and put a space before version marks (`.t-mark-rev`).

### P1-2 Stored pages are never re-parsed after parser fixes (resolved M4)

The local `vector` record (downloaded 2026-11-19) still renders the
declaration as a table, while the current parser already produces
`CodeBlock`s for the same HTML. There is no parser/format version in the
`documents` table, so users keep stale output until they happen to run
`cxxman update`. Fix: store a `parser_version` (or `format_version`)
column with an explicit migration; when a stored page is older than the
current version, print a one-line hint ("page was parsed by an older
cxxman; run `cxxman update vector`"). Do not re-parse on the read path
(raw HTML is not stored, and the read path must not parse HTML).

Resolved in M4: `format_version` now identifies record format 3. Existing
records retain their older marker through the metadata-column migration;
page reads remain local and show a targeted `cxxman update <topic>` hint.

### P1-3 Test fixtures do not represent real pages

All six files in `tests/fixtures/` are synthetic and use none of the real
markup (`t-dcl-begin` rows with revision cells, `t-li1`, `t-dsc` member
tables, `t-rev-begin`, `t-lines`, maintenance boxes). Fix: save trimmed
copies of the real pages (at least `vector`, `sort`, `optional`,
`string`, `vector::push_back`) and write accuracy tests against them:
exact signatures with overload numbers and availability, presence of
INVALIDATION for `vector`, the `t-li1` description, no duplicate rows,
no concatenated names. Keep the synthetic fixtures for edge cases.
Check cppreference's license (CC-BY-SA 3.0 / GFDL) and add attribution
next to the fixtures.

### P1-4 SYNOPSIS comes after DESCRIPTION, and there is no NAME line

Resolved in M3 under the approved quick-card layout: the title line now
contains the source purpose, followed by `#include` and SYNOPSIS. The
project direction uses this compact line instead of a separate NAME heading.

`_SECTION_ORDER` (`manual.py:64`) starts with DESCRIPTION. Unix man pages
use NAME, SYNOPSIS, DESCRIPTION. The renderer prints the bare title
instead of a NAME section. Fix: render `NAME` as
`std::vector - <first sentence of the t-li1/first description
paragraph>` when such a sentence exists (do not synthesize one), then
LIBRARY (`#include <vector>`), then SYNOPSIS, then DESCRIPTION.

### P1-5 Article noise is not filtered

Resolved for the quick card in M3: M2 filters maintenance boxes and
citation back-links. Implementation notes are selected only into NOTES,
and member listings use names without the repeated member-kind suffix.
Generic source content remains available through the full view.

Still rendered on real pages:
- Maintenance boxes ("This section is incomplete Reason: ...").
  Identify the class in a real fixture and add it to `_SKIP_CLASSES`.
- Footnote back-links (`1. ↑ In libstdc++, ...`) — implementation notes
  shown as if universal. Strip the `↑` marker; keep the note under NOTES
  only, clearly attributed.
- The repeated `(public member function)` / `(class template)` suffix in
  every member/see-also row. Move it to a dim style or drop it in the
  concise view.

### P1-6 DESCRIPTION is not concise

Resolved in M3: the card selects the purpose sentence, at most one
description paragraph for classes, and the explicit source complexity list
where available. Long examples and prose are shown in full mode through
hints; no sentence or example is truncated.

For `vector`, DESCRIPTION is ~45 lines, including a complexity bullet
list and a constexpr notice. Suggested deterministic rule: show the
`t-li1` sentence(s) and the first source paragraph; route the
complexity list to COMPLEXITY only when it is explicitly the source's
complexity list; keep the rest available via a detail view (P2-1).
Never shorten a requirement, guarantee, or invalidation statement.

### P2-1 No way to see preserved content (resolved M5)

Non-member functions, specializations, defect reports, and other
omitted blocks are stored but unreachable. `AGENTS.md` already lists
`--detail` as a possible option; it is now justified: render the full
generic `Document` with the existing `render_document`
(`src/cxxman/ui/renderer.py:41`), which is currently unused by the CLI.
Resolved in M5: `cxxman <topic> --full` renders the complete stored
generic document, and `--example` shows the complete saved example section.

### P2-2 Broken pipe traceback (resolved M5)

`cxxman vector | Select-Object -First 20` (or `| head`) ends with a
Python traceback (`OSError: [Errno 22] Invalid argument` on Windows,
`BrokenPipeError` on POSIX). Resolved in M5: the CLI boundary catches these
pipe-close errors, redirects stdout to the null device, and exits quietly.

### P2-3 One failed page aborts the whole catalog update

`update_documents` (`src/cxxman/core/update.py:46`) saves only after
every page succeeds. `cxxman update` with no arguments downloads 50
pages; one 404/timeout discards all 49 good downloads. The
all-or-nothing guarantee only needs to hold per page (a failed page
must not overwrite its previous good version). Option: save each
successful page in its own transaction, collect failures, report them,
and exit non-zero if any failed. Keep the existing "failed update keeps
the last valid page" test.

### P2-4 Small items

- `update.py:43` User-Agent says `cxxman/0.1`; derive it from
  `cxxman.__version__`.
- Members table shows group rows ("Element access", "Iterators") as data
  rows; render them as sub-headings. Note `_SECTION_TITLES` already has
  ITERATORS/CAPACITY/ELEMENT ACCESS/MODIFIERS, but on real pages these
  are table rows, not headings, so those mappings never fire.
- `cxxman list` caps at 100 silently; fine for a 50-page catalog, but
  print "(showing first N)" if the cap is hit.
- `self-update` verifies SHA-256 from the same GitHub release. That is
  integrity, not authenticity; document it as a known limitation.

## Suggested milestone order

> Superseded: the owner chose a shorter "quick card" default and
> whole-library coverage. Use the milestone order in
> `docs/quick-reference-spec.md` section 6; the list below is kept for
> history. All findings above still apply.
>
> Also verified on 2026-11-20: all 50 catalog URLs resolve to the
> expected cppreference page (HTTP 200, no redirect).

1. **M1 Real fixtures** (P1-3) — prerequisite; no behavior change.
2. **M2 Parser fidelity** (P0-2, P0-3, P0-4, P1-1, P1-5 filtering) with tests
   on the real fixtures.
3. **M3 Projection mapping and order** (P0-1, P1-4, P1-6).
4. **M4 Declaration metadata** (P0-3) plus storage format version and
   stale-page hint (P1-2), with an explicit migration.
5. **M5 CLI robustness** (P2-2, P2-3, P2-4).
6. **M6 `--detail`** (P2-1).

M1, M2, and M3 are complete as of 2026-11-21. M2 resolved P0-2, P0-3,
P0-4, and P1-1. M3 resolved P0-1, P1-4, P1-5, and P1-6 for the default
quick card. The real-fixture projection xfail was converted to an exact
passing invalidation assertion; no expected failures remain.

Each milestone should leave the suite green and update this file's
findings with their status.
