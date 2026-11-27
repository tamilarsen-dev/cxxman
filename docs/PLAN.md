# cxxman v1 — product and implementation plan

Status: approved direction, 2026-11-26 (rev 2). This replaces the Python
scraper (v0.3.x), which is kept on branch `legacy-python` (`027a863`).

## Contents

1. Product
2. Feature map
3. CLI reference
4. UI/UX design
5. Content model
6. Following C++ and cppreference
7. Architecture
8. Algorithms
9. Updating the app
10. Testing
11. CI, release, installers
12. Milestones and coverage tiers
13. Later
14. Decisions log

---

## 1. Product

**cxxman: fast, curated C++ standard-library reference cards in the
terminal.** Think `tldr` with the depth of a good cheat sheet, not
`man`. You get one screen, examples first, the pitfalls that actually
bite, and a link to cppreference for the full story.

```
$ cxxman sort        # the card for std::sort, under 10 ms
$ cxxman             # interactive fuzzy finder with live preview
$ cxxman -s "remove duplicates"
$ cxxman -r split string
$ cxxman -c map unordered_map
```

### 1.1 What it is not

- Not a cppreference mirror. No scraping at runtime, no stored
  cppreference prose. cppreference is a source we verify against and
  link to (§6).
- Not exhaustive per entity. A card covers the 90% case; `--web` opens
  the full page.
- No network use except the explicit `update` commands (§9).

### 1.2 Principles (in priority order)

1. **Correct.** Every code snippet compiles in CI on GCC, Clang and
   MSVC at the standard it claims. Facts are verified against
   cppreference or the standard draft and re-checked when upstream
   changes (§6). If unsure, leave it out.
2. **Fast.** A card appears in under 10 ms. No data parsing at runtime.
   The only file read is the optional config.
3. **Short.** A card fits one screen (about 24 lines at 80 columns).
   Budgets are enforced by the linter.
4. **Beautiful and calm.** Consistent layouts, semantic color, graceful
   degradation from truecolor to no color, and nothing that blinks or
   nags (§4).
5. **Useful when wrong.** "Did you mean" on typos; header + link for
   known names without a card.
6. **Complete in breadth.** Every standard header and every name in it
   resolves from day one (§5.6); card depth grows by tier (§12.2).

---

## 2. Feature map

| Feature | Command | Why a dev wants it |
|---|---|---|
| Card | `cxxman <name>` | Header, call shapes, examples, gotchas in one screen |
| Interactive finder | `cxxman` (tty, no args) | Browse/fuzzy-find with live preview, no exact name needed |
| Task search | `cxxman -s <words>` | "remove duplicates" → `unique`; find by intent |
| Recipes | `cxxman -r <words>` | Common tasks across entities: split, trim, read file, join |
| Compare | `cxxman -c <a> <b>` / `-c <topic>` | "Which one do I use?": map vs unordered_map, unique_ptr vs shared_ptr |
| Header index | `cxxman -H <header>` | What's in `<algorithm>`, grouped |
| What's new | `cxxman --new 23` | Everything added in C++23, by header |
| Can I use it? | shown on the card; `--support` | Minimum GCC/Clang/MSVC version and the feature-test macro |
| Std filter | `--std 17` / config | Marks or hides anything newer than your project's standard |
| Examples only | `-e` | All examples, untruncated |
| Signatures only | `--sig` | Quick shape check |
| One-liner | `-b` | `std::sort <algorithm> — Sort a range…` |
| Include line | `--include` | Prints `#include <algorithm>`, handy for editor snippets |
| Copy | `--copy [N]` | Example N (default 1) to the clipboard |
| Open docs | `--web` / `--url` | Full reference in the browser |
| JSON | `--json` | Editor/LSP integrations |
| Coverage | `--coverage` | What's carded vs indexed per header |
| Update | `cxxman update` | Verified self-update from GitHub Releases |
| Changelog | `--changelog` | What changed in this version (cards added/updated) |
| Config | `--config`, `--config init` | Theme, default std, width, hyperlinks… |
| Completions | `--completions <shell>` | bash, zsh, fish, PowerShell, with live name completion |

---

## 3. CLI reference

### 3.1 Synopsis

```
cxxman [options] <name>                 card
cxxman [options]                        interactive finder (stdout is a tty) / usage (otherwise)
cxxman -s, --search <words...>          task search over cards, recipes, compares, index
cxxman -r, --recipe <words...>          recipe by id or best search match
cxxman -c, --compare <a> <b> [<c>]      comparison by subjects, or -c <topic-id>
cxxman -H, --header <header>            header index
cxxman --new <std>                      what's new in a standard (11..26)
cxxman --list [--kind K] [--header H] [--since S]
cxxman --coverage [--missing <header>]
cxxman update [--check] [--pre] [--quiet]
cxxman check-updates | self-update      v0.3 aliases of `update --check` / `update`
cxxman --changelog | --version | --help
cxxman --config [init|path]
cxxman --completions bash|zsh|fish|powershell
```

### 3.2 Card modifiers

`-e/--examples`, `--sig`, `-b/--brief`, `--include`, `--support`,
`--copy [N]`, `--web`, `--url`, `--json`.

### 3.3 Global options

| Option | Default | Notes |
|---|---|---|
| `--color auto\|always\|never` | auto | Honors `NO_COLOR`, `CLICOLOR_FORCE` |
| `--theme dark\|light\|mono` | dark | §4.2 |
| `--std 11..26` | none | Marks newer items; with `--strict`, hides them |
| `--width N` | terminal width, max 100, min 60 | |
| `--ascii` | auto | No Unicode box/symbols |
| `--no-hyperlinks` | links on where supported | OSC 8 (§4.7) |
| `--pager auto\|always\|never` | auto | Built-in pager for long output (§4.8) |

Precedence: CLI flag > `CXXMAN_OPTS` env (same syntax as flags) >
config file > built-in default. `CXXMAN_STD` is a shortcut for `--std`.

### 3.4 Name resolution

1. Normalize: strip a leading `std::`, trim, accept `.` as `::`
   (`vector.push_back`).
2. Exact match on card `id`, then on aliases (case-sensitive, then
   case-insensitive).
3. A bare member name shared by several classes (`size`, `find`)
   lists the candidates and exits 2. A free-function card with that name
   wins, with an "also:" line listing the member candidates.
4. A known index name without a card prints its header, `since` (if
   known) and the cppreference URL, and exits 0. `--web` works.
5. Otherwise print up to 5 fuzzy suggestions and exit 1.

Recipes and compares are never resolved by bare name; they need `-r` or
`-c`. This keeps `cxxman <name>` predictable.

### 3.5 Exit codes

0 ok · 1 not found / no results · 2 ambiguous or usage · 3 internal ·
4 update failed. A broken pipe exits 0 silently.

---

## 4. UI/UX design

### 4.1 Design language

- **One idea per line.** No paragraphs on cards. Prose lines are
  ≤ 72 characters by schema, so wrapping is rare.
- **Hierarchy by weight and color, not by boxes.** Cards use
  whitespace and alignment. Boxes appear only in the interactive finder
  and in compare tables, where they carry structure.
- **Semantic color.** Color encodes meaning (header, signature, UB) the
  same way everywhere. Nothing depends on color alone: gotchas also have
  a symbol, and `mono` stays fully readable.
- **Stable layout.** Same section order on every card; empty sections
  disappear without leaving gaps.
- **Quiet by default.** No banners, tips, update nags, or spinners
  (except a one-line progress indicator during `update`).

### 4.2 Color roles and themes

The renderer uses named roles, and themes map roles to colors:

| Role | Used for | dark (truecolor) | light | 16-color fallback |
|---|---|---|---|---|
| `title` | entity name | bold default fg | bold default fg | bold |
| `header` | `<algorithm>` | `#56b6c2` | `#0e7490` | cyan |
| `badge` | `C++20`, `constexpr C++20` | `#7f848e` | `#6b7280` | bright black |
| `sig` | signature text | `#98c379` | `#15803d` | green |
| `note` | signature notes, comments | `#7f848e` italic | `#6b7280` italic | bright black |
| `kw` / `str` / `num` / `type` | highlighting | `#c678dd` / `#98c379` / `#d19a66` / `#56b6c2` | darker variants | magenta / green / yellow / cyan |
| `ub` | `✗` gotchas | `#e06c75` | `#b91c1c` | red |
| `warn` | `!` gotchas | `#e5c07b` | `#a16207` | yellow |
| `perf` | `~` gotchas | `#61afef` | `#1d4ed8` | blue |
| `link` | names with a card | bold + underline (hyperlink) | same | bold |
| `newer` | items newer than `--std` | badge colour + `⚑` | same | bright black + `!` |
| `accent` | finder selection, prompt | `#61afef` bg / fg | `#dbeafe` bg | reverse video |

- Color depth is detected from `COLORTERM` (`truecolor`/`24bit`), then
  `TERM` (`*256color*`) or `WT_SESSION`/`TERM_PROGRAM` (truecolor), and
  falls back to 16 colors. Truecolor values map to the nearest 256-color
  index automatically.
- `mono` theme: bold/dim/underline only.
- Themes are compiled in. Custom themes are a later feature (§13).

### 4.3 Function / algorithm / member card

```
std::sort  <algorithm>                    C++98 · constexpr C++20 · parallel C++17
Sort a range in ascending order. Not stable.

  sort(first, last)
  sort(first, last, comp)                 comp(a, b) -> bool: true if a goes before b
  ranges::sort(r, comp = {}, proj = {})   C++20 · returns iterator to end of r

  std::sort(v.begin(), v.end());                     // ascending
  std::sort(v.begin(), v.end(), std::greater<>{});   // descending
  std::ranges::sort(people, {}, &Person::age);       // by member  C++20

  ! needs random-access iterators: for std::list use l.sort()
  ✗ comp must be a strict weak ordering: use <, never <=
  ! equal elements may change order: use stable_sort to keep it

O(n log n) comparisons
see: stable_sort partial_sort nth_element is_sorted          cxxman sort --web
```

Rules:
- **Title line:** name (`title`), header (`header`), and badges
  right-aligned (`badge`). If they don't fit, the badges wrap to line 2,
  right-aligned.
- **Signatures:** `sig`; notes in `note`, aligned in a second column if
  every note fits; otherwise each note goes on its own indented line.
- **Examples:** highlighted; `// comment` aligned per card; an example
  `std` badge follows its comment.
- **Gotchas:** `✗` ub, `!` warn, `~` perf (ASCII: `x`, `!`, `~`).
- **Support line** (when present and `--std` is C++20+ or `--support`
  is given): `gcc 13 · clang 17 · msvc 19.29 · __cpp_lib_format` in
  `badge`.
- **Footer:** `see:` names with cards are `link`. The `--web` hint is
  right-aligned.
- **`--std 17`:** signatures, examples, members and badges newer than
  C++17 get `⚑ C++20` in `newer`. With `--strict`, they are hidden and
  a dim line reads `3 items need C++20+ (hidden by --std 17 --strict)`.

### 4.4 Class card

```
std::vector<T>  <vector>                                          C++98
Dynamic array with contiguous storage. The default container.

  vector<T>
  pmr::vector<T>              C++17 · uses a polymorphic allocator

  Access       at  operator[]  front  back  data
  Capacity     empty  size  reserve  capacity  shrink_to_fit
  Modifiers    push_back  emplace_back  pop_back  insert  emplace
               erase  clear  resize  swap
  Non-members  std::erase  std::erase_if                        C++20

  std::vector<int> v{1, 2, 3};          // init list
  v.push_back(4);                       // append
  v.emplace_back(5);
  v.reserve(1000);                      // avoid reallocations
  for (int x : v) { /* ... */ }         // iterate

  ! growth may reallocate: all iterators, pointers and references die
  ✗ v[i] is unchecked; v.at(i) throws std::out_of_range
  ! vector<bool> is packed bits, not a container of bool
  ~ reserve(n) changes capacity only; resize(n) creates n elements

see: array deque span list              cxxman vector::<member> · --web
```

Members with their own card render as `link`.

### 4.5 Other layouts

**Header index** (`-H algorithm`):
```
<algorithm>  Generic algorithms over ranges: search, sort, modify, partition.

  Search     find  find_if  count  count_if  search  binary_search
             lower_bound  upper_bound  any_of  all_of  none_of
  Sort       sort  stable_sort  partial_sort  nth_element  is_sorted
  Modify     copy  copy_if  transform  fill  replace  remove  remove_if
             unique  reverse  rotate  shuffle
  Min / max  min  max  minmax  min_element  max_element  clamp
  Other      adjacent_find  ...  (index names not in any group)

38 of 105 names have cards                          cxxman -H algorithm --web
```

**Search** (`-s remove duplicates`): one result per line, with a kind
marker. The summary is truncated with `…`. At most 10 results.
```
  unique                  <algorithm>  Remove consecutive duplicates. Does not shrink…
  vector::erase           <vector>     Remove elements at a position or in a range.
# split-string            recipe       Break "a,b,c" into parts. Pick by standard…
≠ set-vs-unordered_set    compare      set vs unordered_set
```

**Recipe** (`-r split string`):
```
Split a string by a delimiter                                       recipe
Break "a,b,c" into parts. Pick by standard and ownership needs.

  Owning parts, any standard                                         C++98
    std::istringstream in(s);
    std::vector<std::string> parts;
    for (std::string part; std::getline(in, part, ','); )
        parts.push_back(part);

  Non-owning views, no allocation                                    C++20
    for (auto part : std::views::split(sv, ',')) { ... }

  ! getline drops a trailing empty field: "a,b," gives 2 parts, not 3
  ! views::split on a string literal also splits off its '\0': use string_view
  ✗ views keep pointing into the source: don't let the string die first

uses: getline istringstream views::split string_view
```

**Compare** (`-c map unordered_map`): a box table that fits the width.
Columns wrap inside their cells. Below 70 columns it degrades to a
stacked list (`Order: map — sorted by key; unordered_map — unspecified`).
```
map vs unordered_map
┌────────────────────┬───────────────────────────┬─────────────────────────────┐
│                    │ map                       │ unordered_map               │
├────────────────────┼───────────────────────────┼─────────────────────────────┤
│ Order              │ sorted by key             │ unspecified                 │
│ Lookup / insert    │ O(log n)                  │ O(1) average, O(n) worst    │
│ Key requires       │ operator< (or comparator) │ std::hash<Key> and ==       │
│ Insert invalidates │ nothing                   │ iterators on rehash         │
│                    │                           │ (references survive)        │
│ Range queries      │ lower_bound, upper_bound… │ none                        │
└────────────────────┴───────────────────────────┴─────────────────────────────┘
→ Default to unordered_map for key lookups in hot paths.
→ Use map for ordered iteration, range queries, or keys without a hash.
```

**What's new** (`--new 23`): grouped by header. Cards are `link`;
index-only names are plain. A count line comes first: `C++23: 41 names
across 17 headers (12 with cards)`.

**Not found:**
```
cxxman: no entry for 'emplce_back'
  did you mean  vector::emplace_back  deque::emplace_back  list::emplace_back
```

**Errors** always start with `cxxman: `, use one line, and end with a
hint when one exists: `cxxman: unknown option '--sgi' (did you mean --sig?)`.

### 4.6 Interactive finder

Started by `cxxman` with no arguments when stdin and stdout are ttys.
It uses the alternate screen buffer and restores the terminal on every
exit path (including Ctrl-C and crashes, via a RAII guard plus signal and
console handlers).

```
 cxxman ❯ sort▏                                                     6 / 2,611
╭─────────────────────────────┬──────────────────────────────────────────────╮
│▌sort           <algorithm>  │ std::sort  <algorithm>        C++98 · cexpr20 │
│ stable_sort    <algorithm>  │ Sort a range in ascending order. Not stable.  │
│ partial_sort   <algorithm>  │                                               │
│ ranges::sort   <algorithm>  │   sort(first, last)                           │
│ is_sorted      <algorithm>  │   sort(first, last, comp)                     │
│#sort-by-key    recipe       │   ...                                         │
╰─────────────────────────────┴──────────────────────────────────────────────╯
 ↑↓ move  ⏎ open  tab preview  ^E examples  ^Y copy  ^W web  ^S std  esc quit
```

- Input is matched with the fuzzy scorer over card ids, aliases, index
  names, recipes and compares. If the query has a space, task search is
  used instead. Prefixes: `<` browses headers (`<alg`), `#` shows recipes
  only, `=` shows compares only.
- Every keystroke re-renders within one frame (≤ 16 ms for ≈ 3k
  items). Only changed lines are redrawn (diff against the previous
  frame buffer), so there is no flicker.
- Layout: side-by-side preview at width ≥ 100; preview below the list at
  60–99; no preview under 60 (Tab toggles it).
- Enter opens the full card in the built-in pager (§4.8); `q` returns
  to the finder with the query kept. `Ctrl-O` prints the card to normal
  stdout and exits (for copying from the scrollback).
- Keys: ↑/↓ or Ctrl-P/N move · PgUp/PgDn · Home/End · Ctrl-U clears ·
  Ctrl-E examples view · Ctrl-Y copies example 1 · Ctrl-W opens the web
  page · Ctrl-S cycles `--std` (none/11/14/17/20/23/26) · Esc or Ctrl-C quits.
- Resize events are handled (`SIGWINCH`; `WINDOW_BUFFER_SIZE_EVENT` on
  Windows).
- Windows input uses `ReadConsoleInputW` (handles arrows and UTF-16
  surrogates); POSIX uses `termios` raw mode and an escape-sequence
  decoder with a 25 ms timeout for a lone Esc.
- Mouse support is not in v1 (§13).
- Config `interactive = false` makes bare `cxxman` print usage instead.

### 4.7 Hyperlinks

The renderer emits OSC 8 hyperlinks (`ESC]8;;URL ESC\text ESC]8;; ESC\`)
for `--web` hints, header names (→ the cppreference header page), and
names with cards (→ their cppreference page). This happens only when
stdout is a tty, color is on, and the terminal is known to support it:
`WT_SESSION`, `TERM_PROGRAM` in {iTerm.app, WezTerm, vscode, ghostty},
`VTE_VERSION ≥ 5000`, or `TERM=xterm-kitty`. Config `hyperlinks =
always|never` overrides.

### 4.8 Built-in pager

`-e`, `--new`, `--list`, `-H` for large headers, and finder "open"
can exceed one screen. When stdout is a tty and the output is taller
than the terminal, a minimal built-in pager is used: space/PgDn,
b/PgUp, j/k, g/G, `/` search with n/N, and q. It reuses the finder's
terminal layer. `--pager never` or a non-tty stdout prints directly. The
built-in pager is used instead of `$PAGER` for consistent behavior on
Windows.

### 4.9 Clipboard (`--copy`, Ctrl-Y)

The clipboard backend is chosen in this order:
- Windows: `OpenClipboard` + `CF_UNICODETEXT`.
- macOS: spawn `pbcopy`.
- Linux: `wl-copy`, then `xclip -selection clipboard`, then `xsel -b`.
- Fallback: OSC 52 (works over SSH in most modern terminals).

It prints `copied example 1 (3 lines)` to stderr.

### 4.10 Accessibility and robustness

- `--ascii` is chosen automatically when the console can't do UTF-8.
- No information is carried by color alone.
- Output is correct at 60 columns; golden tests run at 60, 80 and 100.
- Piped output never contains escape codes unless `--color always`.

---

## 5. Content model

All content is TOML in `data/`, compiled into the binary by
`cxxman-gen` (§7.3). The seed files are the format reference:

```
data/entries/algorithm/sort.toml          card (function)
data/entries/algorithm/unique.toml        card (function)
data/entries/vector/vector.toml           card (class)
data/entries/vector/vector-erase.toml     card (member)
data/entries/optional/optional.toml       card (class)
data/recipes/split-string.toml            recipe
data/compare/map-vs-unordered_map.toml    comparison
data/headers/algorithm.toml               header groups
data/index/names.tsv                      every std name (§5.6)
data/index/upstream.lock                  verification state (§6.3)
```

### 5.1 Entry (card) fields

| Field | Req | Type | Rule |
|---|---|---|---|
| `id` | ✓ | string | Lookup key without `std::`. Members: `parent::name`. Unique across ids + aliases. |
| `name` | ✓ | string | Display name with `std::`. |
| `kind` | ✓ | enum | `function` `algorithm` `class` `member` `alias` `trait` `concept` `constant` `macro` `object` `enum` |
| `header` | ✓ | string | Without brackets; must be a header in the index. |
| `since` | ✓ | Std | `C++98` `C++11` `C++14` `C++17` `C++20` `C++23` `C++26` (`C++29` reserved) |
| `summary` | ✓ | string | ≤ 72 chars, ends with `.` |
| `url` | ✓ | string | Path after the base `https://en.cppreference.com/cpp/` (one constant in code). |
| `parent` | members | string | `id` of the class card. Required iff `kind = member`. |
| `badges` | | string[] | ≤ 3, e.g. `constexpr C++20`, `parallel C++17`, `deprecated C++17`, `removed C++20`. |
| `aliases` | | string[] | Extra lookup keys (`ranges::sort`, `find_if`). |
| `tags` | | string[] | ≤ 12 search phrases. |
| `complexity` | | string | ≤ 60 chars. |
| `see_also` | | string[] | ≤ 6, ids or index names. |
| `verified` | | date | Date the facts were last checked against upstream (§6). Absent = unverified. |
| `[support]` | | table | `gcc`, `clang`, `msvc`, `apple_clang`: min version strings; `ftm`: feature-test macro (`__cpp_lib_format`); `ftm_value`: e.g. `201907L`. Only from cppreference compiler-support data. |
| `[[signature]]` | ✓ | table[] | 1–5: `text` ≤ 60, `note` ≤ 50, `since`. |
| `[[member_group]]` | classes | table[] | ≤ 6: `title`, `members` (names), `since`. |
| `[[example]]` | ✓ | table[] | 1–4: `code` ≤ 6 lines × 64 chars, `comment` ≤ 24, `std`. |
| `[[gotcha]]` | | table[] | ≤ 4: `level` `ub`/`warn`/`perf`, `text` ≤ 72. |
| `[test]` | ✓ | table | `includes`, `global`, `prelude`, `chain`, `skip` (§5.5). |

### 5.2 Recipe fields

| Field | Req | Rule |
|---|---|---|
| `id` | ✓ | kebab-case task name, unique among recipes |
| `title` | ✓ | ≤ 60 chars, imperative ("Split a string by a delimiter") |
| `summary` | ✓ | ≤ 72 chars |
| `tags` | ✓ | ≤ 12 |
| `uses` | | ids or index names used by the variants; render as links |
| `[[variant]]` | ✓ | 1–3: `title` ≤ 40, `since` (Std), `code` ≤ 8 lines × 64 chars |
| `[[gotcha]]` | | ≤ 4, same as cards |
| `[test]` | ✓ | same as cards; each variant is a test (§5.5) |

### 5.3 Compare fields

| Field | Req | Rule |
|---|---|---|
| `id` | ✓ | `a-vs-b` or a topic (`smart-pointers`) |
| `title` | ✓ | ≤ 60 |
| `subjects` | ✓ | 2–4 ids or index names; `-c a b` matches any order |
| `tags` | | ≤ 12 |
| `[[row]]` | ✓ | 3–8: `label` ≤ 20, `values` (one per subject, each ≤ 48) |
| `[[verdict]]` | ✓ | 1–3: `text` ≤ 72 |

Compare tables contain facts that are checked like card facts (§6).

### 5.4 Header file fields

`header`, `summary` (≤ 72), and `[[group]]` with `title` and `names`
(ids or index names). Every name must resolve.

### 5.5 Snippet tests

Every card example and every recipe variant is compiled in CI as its
own translation unit:

```
#include <each test.includes>
<test.global>
int main() {
<test.prelude>
<earlier snippets of the same file, only if test.chain = true>
<this snippet>
}
```

- Standard: the highest of C++17 (floor), the card `since`, and the
  snippet's `std`/`since`.
- Flags: GCC/Clang `-Wall -Wextra -Werror -Wno-unused-variable
  -Wno-unused-but-set-variable`; MSVC `/W4 /WX /wd4189 /permissive-`.
- `skip = [{ compiler = "msvc", reason = "..." }]` is for real toolchain
  gaps only. C++26 features without compiler support use the reason
  `no compiler support as of <date>`. CI prints every skip in its
  summary so skips get revisited.
- Snippets must not rely on undefined behavior. They are compiled, not
  run, in v1.

`docs/reference/check_examples_prototype.py` is the prototype used to
validate the seeds. M1 replaces it with the CMake pipeline.

### 5.6 Name index

`data/index/names.tsv` has the columns `name	header	kind	since	url`, one row
per standard-library name. It is generated once in M1 from
`legacy-python:src/cxxman/data/catalog.json` (2,496 names, 111 headers,
including C++26 headers such as `<hive>`, `<inplace_vector>`, `<simd>`,
`<meta>`, `<contracts>`), copying only `topic` (+ aliases), `header`,
`entity_kind` and `source_path`.

- **Never copy `purpose`.** It is cppreference prose.
- `since` stays empty until known. `tools/upstream` fills it (§6.2).
- Fix by hand or drop the one legacy entry without a header.
- After M1 the index is maintained by `tools/upstream/sync_index.py`
  (§6.2) plus hand edits.

### 5.7 Linter (`cxxman-gen --check`)

Errors: missing/unknown fields, any budget exceeded, duplicate ids,
alias collisions, missing `parent`, unresolved `see_also`/`uses`/group
names/compare subjects, a header not in the index, tabs or trailing
whitespace in code, a summary without a final `.`, `skip` without a
reason, `verified` in the future, `[support]` values that don't look like
versions.

Warnings (printed, not fatal): a card without gotchas, a class without
`member_group`, empty `tags`, missing `verified`.

---

## 6. Following C++ and cppreference

cxxman's content is ours, but its facts must track two moving targets:
**new standards** (new names, headers, deprecations) and **cppreference
corrections** (fixed complexity, added constexpr, compiler-support
data). Tracking is maintainer-side and automated, and it never runs on
user machines. Users get the results through `cxxman update`.

### 6.1 Maintainer tools

`tools/upstream/` contains Python 3.12+ scripts that use the **standard
library only** (`urllib`, `html.parser`, `json`, `hashlib`). These are
the only Python files in the repo, and they never ship in releases.
Shared rules:
- They make polite requests: ≥ 1 s apart, `User-Agent:
  cxxman-upstream/<ver> (+https://github.com/tamilarsen-dev/cxxman)`, a
  conditional GET (ETag/Last-Modified) through an on-disk cache in
  `.upstream-cache/` (gitignored), and at most ~600 requests per run.
- They store **no cppreference prose** in the repo, only names, URLs,
  headers, standard versions, compiler-version numbers, feature-test
  macros, and content hashes.
- **Probe first (start of M8):** check what the site serves: MediaWiki
  `api.php` revision IDs, `action=raw`, or HTML only. Record the method in
  §14. Revision IDs are preferred; otherwise use a SHA-256 of the
  normalized main-content text (strip navigation, whitespace and
  revision footers).

### 6.2 Scripts

| Script | Does | Output |
|---|---|---|
| `sync_index.py` | Reads the header pages / symbol index and diffs names, headers, kinds and `since` against `names.tsv` | Rewrites `names.tsv` in place (`git diff` is the review) and prints added/removed/changed names |
| `check_drift.py` | For every card/compare URL, compares the upstream revision/hash with `upstream.lock` | A list of **stale** items: the upstream page changed after `verified` |
| `sync_support.py` | Reads the compiler-support pages and the library feature-test macro table | Proposes `[support]` values per card; with `--apply`, writes only missing values; changed values are reported, never auto-applied |
| `accept.py <id...>` | After a human/agent re-verifies a card | Updates `upstream.lock` with the current revision and bumps the card's `verified` date |
| `report.py` | Runs all of the above read-only | A markdown report (stdout) |

### 6.3 `upstream.lock`

A TSV with the columns `url	rev_or_hash	checked	verified_by`. One row per
URL used by a card or compare. It is committed and changes only through
`accept.py`.

### 6.4 Weekly workflow

`.github/workflows/upstream.yml` runs on `schedule` (Mondays 02:00 UTC)
and `workflow_dispatch`. It runs `report.py` and creates or updates a
single open issue titled **"Upstream changes"** with the report. It uses
`gh` with `permissions: issues: write`. It never commits or pushes; a
human or agent acts on the issue.

Agent procedure for a stale card (also in CLAUDE.md):
1. Fetch the upstream page.
2. Re-check every fact on the card.
3. Fix the TOML in our own words.
4. Run the snippet tests.
5. Run `accept.py <id>`.
6. Commit with "Re-verify <id> against upstream".

### 6.5 When a new standard ships (checklist)

1. Add the enum value in `gen/schema.hpp` and `src/model/model.hpp`
   (`Std::Cpp29`), and extend the linter's allowed `since` values and the
   `--std`/`--new` arguments. Do this via one constant list, not
   scattered literals.
2. Run `sync_index.py` to pick up new headers and names.
3. Add header files for new headers. Add cards by tier (§12.2) and
   `skip` reasons where compilers lag.
4. Re-run `sync_support.py` as compilers catch up; remove skips when CI
   compilers support a feature (the linter lists skips older than 180
   days as warnings).
5. Release a minor version. `--changelog` and `--new <std>` show users
   what arrived.

### 6.6 Changelog for users

`CHANGELOG.md` has one section per version, with these subsections: *New
cards*, *Updated cards*, *New recipes/compares*, *App*. The generator
embeds the current version's section, and `--changelog` prints it. After
a successful `update`, cxxman prints the new version's section headline
counts (`v1.3.0: 24 new cards, 9 updated`).

---

## 7. Architecture

### 7.1 Language and toolchain

- C++20, CMake ≥ 3.25, `CMakePresets.json`.
- Compilers: GCC ≥ 13, Clang ≥ 17, MSVC 19.38+ (VS 2022 17.8+),
  AppleClang ≥ 15.
- Targets: Windows x64/x86, Linux x64, macOS x64/arm64.
- **The runtime has no third-party dependencies.** At build time,
  [toml++](https://github.com/marzer/tomlplusplus) is used for the
  generator and [doctest](https://github.com/doctest/doctest) for tests,
  both pinned and fetched with `FetchContent`. The update and clipboard
  features use only OS facilities: WinHTTP, `tar.exe` and the clipboard
  API on Windows; `curl`, `tar` and `pbcopy`/`wl-copy`/`xclip` on
  POSIX.
- `std::format` is allowed; `std::print` is not (it would raise the floor
  to C++23).
- Platform-specific code lives only in `src/term/`, `src/sys/`,
  `src/net/` and `src/update/`, inside `.cpp` files behind small
  interfaces.

### 7.2 Repository layout

```
CMakeLists.txt  CMakePresets.json  CLAUDE.md  README.md  CHANGELOG.md
LICENSE                         MIT (code)
data/LICENSE                    CC BY 4.0 (content)
data/entries/ recipes/ compare/ headers/ index/
gen/                            cxxman-gen: TOML -> data.cpp, lint, snippet TUs
  main.cpp schema.hpp parse.cpp lint.cpp emit.cpp
src/
  main.cpp                      dispatch
  app/options.{hpp,cpp}         CLI + CXXMAN_OPTS + config merge
  app/config.{hpp,cpp}          tiny key = value config reader/writer
  model/model.hpp               POD structs used by the generated data
  model/lookup.{hpp,cpp}        normalize, resolve, ambiguity, fallback
  search/fuzzy.{hpp,cpp}        fuzzy scorer
  search/search.{hpp,cpp}       task search
  render/style.{hpp,cpp}        roles, themes, color depth, OSC 8
  render/layout.{hpp,cpp}       text measuring (display width), wrapping, alignment
  render/card.cpp recipe.cpp compare.cpp header.cpp lists.cpp json.cpp
  render/highlight.{hpp,cpp}    C++ lexer for coloring
  term/terminal.{hpp,cpp}       tty, size, VT, UTF-8, raw mode, alt screen, resize, keys
  term/screen.{hpp,cpp}         frame buffer with line diffing
  tui/finder.{hpp,cpp}          interactive finder
  tui/pager.{hpp,cpp}           built-in pager
  sys/clipboard.{hpp,cpp}       clipboard backends + OSC 52
  sys/browser.{hpp,cpp}         ShellExecuteW / open / xdg-open
  sys/paths.{hpp,cpp}           config dir, exe path, temp dir
  sys/process.{hpp,cpp}         spawn without a shell (CreateProcessW / posix_spawnp)
  net/http.{hpp,cpp}            WinHTTP / curl: head-redirect, download to memory
  update/sha256.{hpp,cpp}  semver.{hpp,cpp}  update.{hpp,cpp}  extract.{hpp,cpp}
tests/                          doctest units; tests/golden/*.txt
tools/upstream/                 maintainer-only Python (stdlib), §6
installer/cxxman.iss            Inno Setup (ported from legacy)
scripts/install.sh  scripts/uninstall.sh
docs/PLAN.md  STYLE.md  JSON.md  reference/
.github/workflows/ci.yml  release.yml  upstream.yml
```

### 7.3 Build pipeline

1. Build `cxxman-gen`.
2. Run `cxxman-gen --check --emit <build>/generated --snippets <build>/snippets data/`:
   - lint (§5.7); errors fail the build;
   - emit `data.cpp` with `constexpr` tables of the model structs. Strings
     are `std::string_view` literals split per field (avoiding MSVC
     C2026). Entries are sorted by id. The file also holds one sorted key
     table (ids, aliases, index names, recipe ids, compare ids → target),
     pre-normalized keys, a pre-tokenized search index (token → postings),
     and the embedded changelog section;
   - emit one `.cpp` per snippet with `#line` directives pointing at the
     TOML file and line, plus `snippets/manifest.cmake`.
3. `add_custom_command` depends on all TOML/TSV files
   (`CONFIGURE_DEPENDS` glob).
4. `cxxman` links `data.cpp`.
5. `CXXMAN_CHECK_SNIPPETS=ON` (CI, and locally when editing `data/`)
   builds every snippet as an object library at its standard.

### 7.4 Model (sketch)

```cpp
enum class Kind : uint8_t { Function, Algorithm, Class, Member, Alias, Trait,
                            Concept, Constant, Macro, Object, Enum };
enum class Std  : uint8_t { Cpp98, Cpp11, Cpp14, Cpp17, Cpp20, Cpp23, Cpp26, Cpp29, Unknown };
enum class Level: uint8_t { Ub, Warn, Perf };

struct Signature   { std::string_view text, note; Std since; };
struct Snippet     { std::string_view title, code, comment; Std std; };
struct Gotcha      { Level level; std::string_view text; };
struct Group       { std::string_view title; std::span<const std::string_view> names; Std since; };
struct Support     { std::string_view gcc, clang, msvc, apple_clang, ftm, ftm_value; };

struct Entry {
  std::string_view id, name, header, summary, complexity, url, parent, verified;
  Kind kind; Std since; Support support;
  std::span<const std::string_view> badges, aliases, tags, see_also;
  std::span<const Signature> signatures;
  std::span<const Group> members;
  std::span<const Snippet> examples;
  std::span<const Gotcha> gotchas;
};
struct Recipe  { std::string_view id, title, summary;
                 std::span<const std::string_view> tags, uses;
                 std::span<const Snippet> variants; std::span<const Gotcha> gotchas; };
struct Row     { std::string_view label; std::span<const std::string_view> values; };
struct Compare { std::string_view id, title; std::span<const std::string_view> subjects, tags, verdicts;
                 std::span<const Row> rows; };
struct IndexName { std::string_view name, header, url; Kind kind; Std since; int32_t entry; };
struct Header  { std::string_view header, summary; std::span<const Group> groups; };
```

There is no allocation at startup. Renderers append to one
`std::string` and write it once (to the pager when paging).

### 7.5 Config file

The config lives at `%APPDATA%\cxxman\config.toml` (Windows) or
`$XDG_CONFIG_HOME/cxxman/config.toml` (default `~/.config/...`, including
on macOS). It uses a flat `key = value` subset (string, int, bool, `#`
comments) and is read by a ~100-line hand-written parser. Unknown keys
produce a one-line warning to stderr; bad values fall back to defaults with
a warning. A missing file is silent.

Keys: `theme`, `color`, `std`, `strict`, `width`, `ascii`, `hyperlinks`,
`pager`, `interactive`. `--config` prints the path and the effective
values with their source (default/config/env/flag). `--config init` writes
a commented default file, and refuses to overwrite an existing one.

---

## 8. Algorithms

### 8.1 Normalization

Use the rules in §3.4. All keys are pre-normalized by the generator.

### 8.2 Fuzzy scorer (finder and did-you-mean)

The score is the best of:
- a case-insensitive exact prefix: 1000 − length difference;
- Damerau–Levenshtein distance d ≤ max(1, len/3): 500 − 100·d;
- a subsequence match: 200 + 15 per match at a word start (after `_`,
  `::`, `<`, or a lower-to-upper case change) + 5 per consecutive run −
  1 per gap character.

Ties prefer cards > recipes/compares > index names, then shorter keys,
then alphabetical order. Deduplicate by target. The finder budget is
≤ 2 ms for ≈ 3k keys; use precomputed lowercase keys and an early exit
on the length bound.

### 8.3 Task search

Tokenize the query into lowercase words and drop stop words. Per
document (card, recipe, compare), sum over the query tokens:
- 30 for an exact id/alias token;
- 25 if the whole query equals a tag phrase;
- 15 for a tag word;
- 8 for a summary/title word;
- 5 for a prefix (≥ 3 chars).

Multiply by 1.5 if every token matched. Index names score 10 per name
token. Use the pre-built postings table from the generator.

Golden acceptance cases:
- `remove duplicates` → `unique` first;
- `dynamic array` → `vector` first;
- `erase element` → `vector::erase` in the top 3;
- `maybe value` → `optional` first;
- `split string` → recipe `split-string` first;
- `hash map` → compare `map-vs-unordered_map` in the top 3.

### 8.4 Highlighter

A single-pass lexer recognizes comments, string/char/raw-string literals,
numbers (including `'` separators and suffixes), a fixed C++23 keyword
list, `std::`-qualified names as `type`, and preprocessor lines. Unknown
input passes through. No regex. It never fails.

### 8.5 Display width

Measure the display width of UTF-8 text (box drawing and symbols = 1;
East Asian wide = 2, via a small compiled table). All alignment uses
display width, never byte length.

---

## 9. Updating the app

Content lives in the binary, so new cards arrive with a new version:
one artifact to verify. Separate data packs were rejected for v1.

- `cxxman update --check` reports either `cxxman 1.2.0 is available
  (you have 1.1.0)` or `up to date`. It exits 0, or 4 on failure.
- `cxxman update` downloads the platform asset and its `.sha256`, and
  verifies the checksum before touching anything. Then:
  - **Windows (installed):** run the setup from `%TEMP%` with
    `/CURRENTUSER /CLOSEAPPLICATIONS /NORESTARTAPPLICATIONS` (as v0.3
    did), adding `/SILENT` when `--quiet` is given, then exit. A Windows
    install counts as "installed" if `unins000.exe` sits next to the exe.
  - **Windows (portable):** download the `.zip`, extract it with
    `tar.exe`, rename the running exe to `cxxman.old.exe`, put the new
    one in place, and delete any leftover `.old.exe` silently on the next
    start.
  - **Linux/macOS:** extract `cxxman` with `tar` into a private temp
    dir, move it next to the target, copy the mode bits, and `rename()`
    it over the target. On `EACCES`, explain the problem and suggest
    `install.sh`.
- **Discovery** does not use the GitHub API (no JSON, no rate limit). It
  requests `https://github.com/tamilarsen-dev/cxxman/releases/latest`
  without following redirects and parses the tag from `Location`. Assets
  are at `.../releases/download/<tag>/<asset>`. Only `github.com/tamilarsen-dev/cxxman/`
  and GitHub's asset hosts (`objects.githubusercontent.com`,
  `release-assets.githubusercontent.com`) are followed.
- SemVer comparison; pre-releases only with `--pre`.
- SHA-256 is an in-house implementation tested with the NIST vectors.
- Progress shows as one line on stderr (`downloading 1.4 MB… 62%`), and
  only on a tty.
- There are no automatic or background checks, ever.
- On the first run of v1 over a v0.3 install, if the old SQLite page
  cache exists, print a one-time note with its path so the user can
  delete it. Never delete it automatically; record that the note was
  shown in the config dir.

---

## 10. Testing

- **Unit (doctest):** normalization, resolution, the fuzzy scorer, search
  ranking, options/env/config precedence, the config parser, wrapping and
  display width, badge overflow, JSON escaping, `--std` marking, SHA-256
  (NIST), SemVer, `Location` parsing, the URL allowlist, key decoding
  (escape sequences → keys), and the screen diff.
- **Golden:** at widths 60/80/100 with `--color never`, render: every seed
  card, the recipe, the compare (table and stacked), `-H algorithm`, the
  search acceptance queries, a miss, an ambiguity, `--new 17`, and
  `--std 17` on `sort`. Also run once with `--color always --theme dark`
  to lock the escape output for one card. Set
  `CXXMAN_UPDATE_GOLDEN=1` to rewrite, and review the diffs.
- **Finder:** drive the TUI through a fake terminal backend (an
  interface in `term/`) with scripted keys, and snapshot the frame
  buffer.
- **Snippets:** all three compilers (§5.5).
- **Smoke (CI, real binary):** `--version`, a card, `| head -1`, a miss
  (exit 1), `--json` parses (CI checks it with `python -m json.tool`), and
  `update --check` against the real GitHub (allowed to fail softly on
  network errors).
- **Performance:** the median of 100 runs of `cxxman sort --color never`
  must be ≤ 20 ms in CI (target 10 ms locally). A finder benchmark
  (scoring all keys) must be ≤ 2 ms.
- **Sanitizers:** GCC ASan+UBSan for the unit, golden and finder tests.

---

## 11. CI, release, installers

### 11.1 CI (`ci.yml`, every push/PR)

The matrix: ubuntu-24.04 (GCC 13, Clang 18), windows-2022 (MSVC x64,
plus an x86 build-only job), macos-14 (AppleClang). Steps: configure,
build (the lint runs as part of it), build the snippets, ctest, smoke.

### 11.2 Release (`release.yml`, tag `v*`)

The workflow fails if the tag ≠ the CMake project version. It builds the
five platforms, signs nothing (as in v0.3), smoke-tests every binary,
builds the Windows installers with Inno Setup, installs and uninstalls
them silently on the runner, and publishes the assets.

### 11.3 Asset contract (must match v0.3.1 exactly)

v0.3.1's updater looks for these names, each with `<name>.sha256` (the
first token is the lowercase hex digest). Changing them strands existing
users.

| Platform key | Assets |
|---|---|
| `windows-x64` | `cxxman-windows-x64-setup.exe`, `cxxman-windows-x64.zip` |
| `windows-x86` | `cxxman-windows-x86-setup.exe`, `cxxman-windows-x86.zip` |
| `linux-x64` | `cxxman-linux-x64.tar.gz` |
| `macos-x64` | `cxxman-macos-x64.tar.gz` |
| `macos-arm64` | `cxxman-macos-arm64.tar.gz` |

- A `.tar.gz` has a regular file `cxxman` **at the archive root**, plus
  `install.sh`, `uninstall.sh`, README.md, LICENSE and
  `data-LICENSE`.
- A `.zip` has `cxxman.exe` at the root.
- The release also ships a combined `SHA256SUMS`.
- Linux: built on ubuntu-22.04 with `-static-libstdc++ -static-libgcc`.
- macOS: unsigned; the README documents the Gatekeeper workaround.

### 11.4 Installers

- **Windows:** Inno Setup, ported from `legacy-python:installer/cxxman.iss`.
  Keep `AppId={{D31D5F93-18BE-4E38-98C1-6D8F9D9425B3}`,
  `DefaultDirName={localappdata}\Programs\cxxman`,
  `PrivilegesRequired=lowest`, and the per-user PATH code, so v1 upgrades
  a v0.3.1 install in place. Use `[InstallDelete]` for the old
  PyInstaller leftovers; check the legacy `.iss` and build layout for
  what v0.3 installed.
- **Linux/macOS:** `install.sh` puts `cxxman` in `~/.local/bin` (or
  `$PREFIX/bin`), warns if that directory is not on PATH, and is
  idempotent. It also prints how to install shell completions.
  `uninstall.sh` reverses it.
- **Upgrade test before v1.0.0:** install the real v0.3.1, run its
  `self-update`, and confirm v1 runs, on Windows and Linux. Record the
  result in the release notes.

---

## 12. Milestones and coverage tiers

### 12.1 Milestones

Do one milestone at a time, in order. Each has tests, and "Done when" is
the definition of done. Mark finished ones with `✅ <date>`.

**M0 — Fresh start.** ✅ 2026-11-27. The cleanup was already done on 2026-11-26:
branch `legacy-python` = `027a863`, the old files were removed, and this
plan was moved to the root (all staged, not committed). Remaining:
commit the cleanup as "Start C++ rewrite; legacy Python code kept on
branch legacy-python"; add `.gitignore` (build/, out/, .vs/, .vscode/,
.idea/, .upstream-cache/, `__pycache__/`); add a CMake skeleton printing
`cxxman 0.9.0-dev`; and add `ci.yml` (build only).
*Done when:* CI is green on all jobs.

**M1 — Data pipeline.** ✅ 2026-11-27. Model structs; `cxxman-gen` (parse all four
file types, the full linter, `data.cpp`, snippet TUs with `#line`);
`names.tsv` from the legacy catalog; `upstream.lock` (empty, header
only); `data/LICENSE`; `CHANGELOG.md`.
*Done when:* all seeds lint clean and all 19 snippets compile on three
compilers; a broken snippet fails with `file.toml:line`; and an unknown
field fails the lint.

**M2 — Core rendering.** ✅ 2026-11-27. Lookup (§3.4), the fuzzy did-you-mean, the
style system (roles, themes, color depth, `--ascii`), the layout engine
(display width, alignment, wrapping), the card and class layouts, the
highlighter, the terminal basics (tty, size, VT, UTF-8, broken pipe),
options + `CXXMAN_OPTS` + config.
*Done when:* the card goldens pass at 60/80/100; the fallback, miss and
ambiguity cases work (use fixtures for ambiguity until content exists);
and startup is ≤ 10 ms locally.

**M3 — Discovery.** ✅ 2026-11-27. `-s` search with the postings index, `-H`,
`--list` filters, `--new`, `--coverage`.
*Done when:* all six search acceptance cases (§8.3) pass as goldens.

**M4 — Output modes and polish.** `-e`, `--sig`, `-b`, `--include`,
`--support`, `--url`, `--web`, `--copy`, `--json` (+ `docs/JSON.md`),
`--std`/`--strict`, OSC 8 hyperlinks, the built-in pager,
`--completions` for four shells (with dynamic name completion via a
hidden `--complete <prefix>`), `--help` (≤ 40 lines, grouped), and
`--changelog`.
*Done when:* every mode has a golden or unit test, and the clipboard and
browser have been manually checked on Windows and one POSIX system
(noted in the commit).

**M5 — Interactive finder.** The finder per §4.6 on top of the terminal
layer, the screen diff, and the fake-terminal tests.
*Done when:* the scripted-key tests pass; resize, Ctrl-C and crash
paths restore the terminal (manual check in Windows Terminal, conhost
and one POSIX terminal); and the scoring benchmark is ≤ 2 ms.

**M6 — Recipes and compares.** Render both, plus their search and
finder integration. Content: 10 recipes (split string, trim, join,
to lower/upper, read whole file, read lines, string ↔ number, iterate
map with structured bindings, remove if, sort by key) and 6 compares
(map vs unordered_map, set vs unordered_set, vector vs deque vs list,
unique_ptr vs shared_ptr vs raw pointer, string vs string_view, sort vs
stable_sort vs partial_sort).
*Done when:* everything compiles and passes in CI, and the compare
goldens pass in both table and stacked form.

**M7 — Update, installers, release pipeline.** SHA-256, HTTP, discovery,
SemVer, `update` and its aliases, all apply paths (§9), the Inno Setup
port, `install.sh`/`uninstall.sh`, and `release.yml` per §11.3.
*Done when:* tag `v0.9.0` publishes every asset with its sidecar; a faked
older version self-updates on Windows (installed + portable) and Linux;
and a corrupted checksum gives exit 4 with nothing changed.

**M8 — Upstream tracking.** Probe cppreference (§6.1), then the five
scripts, `upstream.yml`, and a first `sync_index.py` run that fills
`since` in `names.tsv`.
*Done when:* `report.py` produces a correct report on the seeds; a
manually edited lock row shows up as stale; and the workflow opens the
issue on a test run.

**M9 — Tier A content (≈ 150 cards).** See §12.2. Every card ends with
`verified` set and is accepted into `upstream.lock`; `sync_support.py`
fills `[support]` for C++17+ entities.
*Done when:* everything passes, and the owner has spot-checked at least
1 in 5 cards with STYLE.md §6.

**M10 — v1.0.0.** README (screenshots of the card, finder and compare;
install; usage; why not cppreference/cppman; contributing), CONTRIBUTING.md,
the v0.3.1 upgrade test, and the `v1.0.0` tag.

**M11 — Tier B: every header (≈ 600 cards, plus recipes/compares as
gaps show up).** Work header by header in the §12.2 order, one header
per commit, with minor releases as batches land.
*Done when:* every header in the index has a header file and its Tier B
cards.

**M12 — Tier C: full coverage.** The remaining names, with shared cards
via aliases. This is ongoing, driven by `--coverage` and the weekly
upstream report.

### 12.2 Coverage tiers

The goal is **every name in every standard header**, delivered in order
of usefulness so every release is good, not half-done.

- **Tier A (≈ 150):** the containers (`vector` `array` `string`
  `string_view` `map` `unordered_map` `set` `unordered_set` `deque` `list`
  `span` `pair` `tuple`) and their key members; the top ~30 algorithms;
  `optional` `variant` `any` `expected`; `unique_ptr` `shared_ptr`
  `make_unique` `make_shared`; `move` `forward` `swap`; `function`;
  `chrono` basics; `filesystem` basics; `format` `print`; `thread`
  `jthread` `mutex` `lock_guard` `scoped_lock` `atomic`; `accumulate`
  `reduce` `iota`; `to_string` `stoi` `from_chars`; the common ranges
  views; `<random>` basics; and the `iostream`/`fstream`/`sstream` basics.
- **Tier B (≈ 600):** the principal entities of **every** header. Header
  order: containers → `<algorithm>` `<numeric>` `<ranges>` → strings and
  text (`<string>` `<string_view>` `<charconv>` `<format>` `<print>`) →
  `<memory>` → `<utility>` `<tuple>` `<functional>` → `<chrono>` →
  `<filesystem>` → I/O → concurrency (`<thread>` `<mutex>` `<atomic>`
  `<condition_variable>` `<future>` `<latch>` `<barrier>` `<semaphore>`
  `<stop_token>`) → `<type_traits>` `<concepts>` → `<iterator>` →
  `<random>` → `<regex>` → errors (`<exception>` `<stdexcept>`
  `<system_error>`) → `<limits>` `<bit>` `<numbers>` → C compatibility
  (`<cmath>` `<cstring>` `<cstdlib>` `<cstdio>` …) → C++23/26 (`<expected>`
  `<mdspan>` `<flat_map>` `<flat_set>` `<generator>` `<stacktrace>`
  `<hive>` `<inplace_vector>` `<simd>` `<meta>` `<contracts>`
  `<debugging>` `<text_encoding>` …).
- **Tier C (the rest, ≈ 1,700 names):** obscure members, macros,
  specializations. Many share one card through `aliases` (all `is_*`
  primary type traits on one card; the `FLT_*` macros on one card), so
  there are far fewer files than names.
- An index name is **covered** when it is a card id or alias.
  `--coverage` shows covered/total per header, with the most uncovered
  first.

---

## 13. Later (not v1)

- Running snippets with an expected `output` field in CI.
- `CXXMAN_PATH`: user or team card directories loaded at runtime (opt-in).
- Custom themes in the config file.
- Mouse support and bookmarks/history in the finder.
- Editor integrations on `--json`: a Neovim plugin (`K` on a symbol) and
  a VS Code hover extension.
- Scoop, winget and Homebrew manifests (alongside §11.4, not instead).
- Language cards (`kind = language`: lambdas, structured bindings,
  `constexpr`, …).
- An Indonesian content translation (`data/i18n/id/`) once English
  content is stable.
- A hidden `--log-misses` flag (opt-in, local file) to prioritize new
  cards.

## 14. Decisions log

| Date | Decision | Why |
|---|---|---|
| 2026-11-26 | Rewrite in C++20; drop the Python scraper | Startup speed, single binary, owned content |
| 2026-11-26 | Curated TOML content, not cppreference text | Brevity needs our own wording; cppreference is CC BY-SA |
| 2026-11-26 | Snippets compiled in CI on 3 compilers | Main guard against wrong content |
| 2026-11-26 | Name index from the legacy catalog | All 2,496 names resolve from day one |
| 2026-11-26 | Self-update + installers kept; v0.3.1 asset names unchanged | Existing users upgrade via their own `self-update` |
| 2026-11-26 | Network only in `update`; OS tools (WinHTTP/curl/tar) | No runtime dependencies; privacy |
| 2026-11-26 | Data embedded; no separate data packs | One artifact to verify |
| 2026-11-26 | Coverage goal: every name, in tiers A/B/C | Each release is good, not half-done |
| 2026-11-26 | Same repo; old code on `legacy-python` | Keeps stars, history and the update channel |
| 2026-11-26 | Upstream tracking with maintainer-only stdlib Python + weekly issue | HTML scraping is a poor fit for C++; never ships; no prose stored |
| 2026-11-26 | Interactive finder, recipes, compares, support data in v1 | The features that save developers the most time |
| 2026-11-26 | Built-in pager instead of `$PAGER` | Same behavior on Windows |
| 2026-11-26 | Config: flat key = value subset, hand-parsed | The only runtime parsing; tiny and fast |
| 2026-11-26 | `dev` preset: platform default generator, Debug, `CMAKE_COMPILE_WARNING_AS_ERROR`; shown version = `project(VERSION)` + `CXXMAN_VERSION_SUFFIX` | Works on every CI runner without a vcvars step; warnings never pile up; the release check compares the tag with `project(VERSION)` alone |
| 2026-11-26 | M0 CI also runs `ctest` (except the x86 build-only job) | One version smoke test costs nothing and keeps `ctest --preset dev` meaningful |
| 2026-11-26 | `names.tsv` import rules: a row per topic and alias; a missing header is taken from another entry with the same `source_path` (203 entries), else the entry is dropped (21); `<>`, `()` and `(std::X)` overload markers are stripped; exposition-only names (`boolean-testable`) are dropped; one row per (name, header, url); legacy kind `unknown` becomes empty; first line holds the column names | The legacy catalog had 224 entries without a header, not one (§5.6), and display-only name forms |
| 2026-11-26 | Hand rows in `names.tsv`: `vector::clear`, `vector::pop_back`, `getline`, `istringstream` | The seeds' `see_also`/`uses` name them and the legacy catalog lacks them |
| 2026-11-26 | `Kind::Unknown` for index names without a kind; never valid on a card | 460+ legacy names have no kind |
| 2026-11-26 | Enum spellings live once, in `src/model/model.hpp`; `gen/schema.hpp` includes it | One list per enum (§6.5) |
| 2026-11-26 | Snippets compile in a nested CMake project (`cmake/snippets`), configured and built at build time by `cmake/check_snippets.cmake`, one object library per standard | The snippet list only exists after `cxxman-gen` runs |
| 2026-11-26 | The linter resolves `see_also`, `uses`, compare subjects and header-file groups, not class `member_group` names | Member groups list member names (`at`, `operator[]`) that are not lookup keys |
| 2026-11-26 | M1's `data.cpp` holds the tables, the key table and the changelog; the search postings come with M3 | The postings layout depends on M3's scoring |
| 2026-11-26 | `cxxman-gen` always lints; `--check` also prints the warnings | Keeps §7.3's command line and makes warnings visible in the build |
| 2026-11-27 | macOS CI runs on `macos-15` (arm64, Xcode 16.4, AppleClang 17) instead of `macos-14` (§11.1); `actions/checkout@v7` | GitHub retired all macOS 14 images on 2026-11-02; checkout v4 runs on the deprecated Node 20. AppleClang 15–16 (§7.1) is no longer tested in CI; the x64 release build (M7) needs `macos-15-intel` |
| 2026-11-26 | Recipe `split-string`: second gotcha shortened to "views::split over a string literal keeps its '\0': use string_view" | It was 74 characters; the budget is 72 |
| 2026-11-27 | Resolution (§3.4): an exact name, card or index, wins and lists same-named members of classes on an `also:` line; a bare name that only classes have is ambiguous (exit 2) with two or more classes and opens directly with one; `X::name` counts as a member only when `X` is a class, so `ranges::sort` is not one | Rule 3 only covered a free-function *card*; index names like `std::erase` need the same treatment |
| 2026-11-27 | Did you mean: the scorer also tries the part after the last `::`; suggestions score ≥ 150 and within 100 of the best; a card shows its shortest key | `emplce_back` must find `vector::emplace_back`; without the window, loose subsequence matches crowd out the one-typo hit |
| 2026-11-27 | Index-only names: header and URL; a name in several headers lists up to 8 (two columns when they fit, else stacked); URLs are never wrapped, so they may pass the width | A wrapped URL is no longer clickable |
| 2026-11-27 | Card layout details: notes that do not all fit go on their own line indented 6; example comments align after the first code line when all fit, else go above the code; code wider than the card wraps at a space with 4 more spaces; badges that do not fit go to line 2, right-aligned; ASCII symbols are `x` `!` `~` `\|` `...` | §4.3 states the aligned case only; a 72-character line at width 60 must still be correct |
| 2026-11-27 | "No entry" and ambiguity messages go to stderr, without color | Errors start with `cxxman:` (§4.5); stdout stays clean for pipes |
| 2026-11-27 | Options of later milestones are recognized and fail with "not available yet"; M2's `--help` is a short version that M4 replaces | Typos still get a "did you mean"; `--help` ≤ 40 lines is M4 work |
| 2026-11-27 | `--width` is clamped to 60–100; when stdout is not a terminal, `COLUMNS` and then 80 are used | The layouts are designed for that range |
| 2026-11-27 | The config file also accepts bare words for strings (`theme = light`); on POSIX, no locale variable set counts as UTF-8 | Friendlier than strict TOML; modern terminals are UTF-8 |
| 2026-11-27 | Model structs give every member a `{}` default | Designated initializers in tests and code stay free of `-Wmissing-field-initializers` |
| 2026-11-27 | Goldens are `tests/golden/<case>[.<width>].txt` holding the command, exit code, stdout and stderr, with ESC shown as `\e`; `.gitattributes` keeps them LF | One file shows everything a case does; readable diffs |
| 2026-11-27 | Startup measured on Windows (Release): `cxxman sort` lives 8.6 ms (median of 100), of which 8.2 ms is the cost of any process (`--version`) | §1.2 asks for under 10 ms; process creation dominates on Windows |
| 2026-11-27 | Task search (§8.3): one tokenizer (`src/search/tokenize.hpp`) for the generator and the query: lowercase ASCII words split at anything but letters and digits, minus a short stop-word list that includes `std`. Per document, each query word counts its best match once (id/alias/compare subject 30, tag word 15, summary/title word 8, prefix of ≥ 3 letters 5); a query equal to a whole tag adds 25 once; index names score 10 per exact name word (no prefixes); the ×1.5 applies to all. Stored scores are doubled so the factor stays an integer | §8.3 says "sum over the query tokens" without saying whether one word can score in several fields; counting the best match keeps a word that sits in id, tags and summary from outweighing two matched words |
| 2026-11-27 | Search postings: a sorted `Term` table (word or whole tag → target, field, index). Index names that a card covers get no postings; one index name in several headers is one result, shown with its first header and "in N headers"; index names show "no card yet" | Results stay one line per thing a user can open; no duplicates for `erase` in 7 headers |
| 2026-11-27 | Discovery lists work on the *known names*: every (name, header) of the index plus cards missing from it (`vector::erase`); a card lends its kind and `since` when the index has none and the header matches. `-H` shows the header file's groups and puts every other name under "Other" ("Names" when there is no header file). `--list --since S` means S or later; `--new S` means exactly S, and with no names it exits 1 ("no names known for C++23 yet") until M8 fills `since`. `--coverage` sorts by uncovered count; `--missing <header>` also works without `--coverage` | The index lacks most `since` values and some carded names; these keep every listing consistent with the cards |
| 2026-11-27 | `--header H` shows header H alone and filters `--list` with it; `--kind` and `--since` only filter `--list`; two discovery actions together are a usage error. All discovery layouts live in `render/lists.cpp` (no separate `header.cpp`). Name columns size to the widest name up to a cap (search 24, `--list` 32); a longer name pushes the rest of its own line | One flag for both meanings of "header" in §3.1; one long name must not widen every line |
| 2026-11-27 | Card modifiers: `-e`, `--sig`, `-b`, `--include` and `--url` (later `--copy`, `--web`, `--json`) pick one output, so two of them are a usage error; `--support` adds the support line to the full card (between the complexity line and `see:`), and says "no support data yet" when the card has none. Modifiers need a card name. On an index-only name, `--include` and `--url` work when the name is in one header (several headers: exit 2, pointing at `cxxman <name>`); the others exit 1 with "has no card yet" | §3.2 lists the modifiers without saying how they combine; §4.3 shows `--support` as a line on the card |
| 2026-11-27 | `-e` prints the title line and every example with code never wrapped (comments go above when they do not fit); `--sig` prints the title line and the signatures; `-b` is `name <header> — summary`, cut to the width with `…` | Unwrapped code can be copied as is |
| 2026-11-27 | `--std` marking: a badge's standard is the `C++NN` at its end (`constexpr C++20`); the card's own `since` badge is marked but never hidden; `⚑` is `!` in ASCII; the `--strict` line goes after the gotchas and names the oldest standard that was hidden | §4.3 says badges are marked without saying how a badge's standard is known; hiding the whole card would hide the name the user asked for |
| 2026-11-27 | `-H` leaves out `ranges::X` when `X` is a name of the same header, and adds one dim line `+ N ranges:: versions of the names above (cxxman --list --header H)`; the footer count still covers every name | `<algorithm>`'s "Other" drops from ~65 to ~27 lines at 80 columns; the card of `X` covers `ranges::X` through its aliases, and nothing disappears without a note |
| 2026-11-27 | `--help` lists every v1 command, including those of later milestones, which say "not available yet" until they land; `--changelog` prints `cxxman <version>`, a blank line and the embedded section as written | The help text is written once; the section's heading is stripped by the generator |
