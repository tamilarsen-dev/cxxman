# cxxman `--json` output

`--json` prints one JSON document on stdout for editor plugins and scripts
(PLAN §13: a Neovim `K` handler, a VS Code hover). This file is the
contract. Schema version: **1**.

## Rules

- Works with `cxxman <name>`, `-s <words>` and `-H <header>`. With any other
  action, or with another card modifier (`-e`, `--sig`, `-b`, `--include`,
  `--url`, `--copy`, `--web`), it is a usage error.
- Output is UTF-8 and never colored. Whitespace and key order are not part
  of the contract.
- Every document has `"schema": 1` and a `"type"`.
- Every key listed for a type is always present. A value that is not known
  is `null`; a list with nothing in it is `[]`.
- Standards are spelled `"C++98"` … `"C++26"`. Kinds are the `kind`
  spellings of PLAN §7.4 (`"function"`, `"class"`, …); an index name whose kind is
  not known has `"kind": null`.
- `--std` and `--strict` do not change the output: every item keeps its
  `since`, so a client can filter.
- Every result, a miss included, is a JSON document on stdout with the
  same exit code as without `--json`. Only usage errors (exit 2: a bad
  option, a missing value) stay plain text on stderr, with stdout empty.
- Text that cannot be UTF-8 (a mistyped name with stray bytes) has each bad
  byte replaced by U+FFFD.
- Compatibility: new keys and new `type` values may be added without a new
  schema version, so clients must ignore what they do not know. Removing
  or renaming a key, or changing what a value means, bumps `schema`.

## `cxxman <name> --json`

The `type` follows name resolution (PLAN §3.4). The exit code is the same
as without `--json`.

### `card` (exit 0)

```json
{
  "schema": 1,
  "type": "card",
  "id": "sort",
  "name": "std::sort",
  "kind": "algorithm",
  "header": "algorithm",
  "parent": null,
  "since": "C++98",
  "badges": ["constexpr C++20", "parallel C++17"],
  "summary": "Sort a range in ascending order. Not stable.",
  "aliases": ["ranges::sort", "std::ranges::sort"],
  "signatures": [
    {"text": "sort(first, last)", "note": null, "since": null},
    {"text": "ranges::sort(r, comp = {}, proj = {})",
     "note": "returns iterator to end of r", "since": "C++20"}
  ],
  "members": [],
  "examples": [
    {"code": "std::sort(v.begin(), v.end());", "comment": "ascending", "since": null}
  ],
  "gotchas": [
    {"level": "ub", "text": "comp must be a strict weak ordering: use <, never <="}
  ],
  "complexity": "O(n log n) comparisons",
  "support": null,
  "see_also": ["stable_sort", "partial_sort", "nth_element", "is_sorted"],
  "url": "https://en.cppreference.com/cpp/algorithm/sort",
  "also": []
}
```

- `parent`: the class id of a member card (`"vector"`), else `null`.
- `members`: for a class, `[{"title": "Access", "names": ["at", …], "since": null}]`.
- `examples[].code` may hold several lines, separated by `\n`.
- `gotchas[].level`: `"ub"`, `"warn"` or `"perf"`.
- `support`: `null`, or `{"gcc", "clang", "apple_clang", "msvc", "ftm",
  "ftm_value"}`, each a string or `null`.
- `also`: members of classes with the same name (`"string::size"`).

### `index` (exit 0): a known name without a card

```json
{
  "schema": 1,
  "type": "index",
  "name": "istringstream",
  "places": [
    {"header": "sstream", "kind": "alias", "since": null,
     "url": "https://en.cppreference.com/cpp/io/basic_istringstream"}
  ],
  "also": []
}
```

A name in several headers has one `places` item per header.

### `ambiguous` (exit 2)

```json
{"schema": 1, "type": "ambiguous", "name": "size",
 "candidates": ["string::size", "vector::size"]}
```

### `missing` (exit 1)

```json
{"schema": 1, "type": "missing", "name": "srot", "suggestions": ["sort"]}
```

## `cxxman -s <words> --json`

```json
{
  "schema": 1,
  "type": "search",
  "query": "remove duplicates",
  "results": [
    {"target": "card", "name": "unique", "header": "algorithm",
     "summary": "Remove consecutive duplicates. Does not shrink the container.",
     "headers": 1},
    {"target": "index", "name": "remove", "header": "algorithm",
     "summary": null, "headers": 1}
  ]
}
```

- Results are in rank order, at most 10, like the text output.
- `target`: `"card"`, `"recipe"`, `"compare"` or `"index"`. The `name` is
  what to pass back: `cxxman <name>`, `cxxman -r <name>` or `cxxman -c <name>`.
- `header`: `null` for recipes and compares. For an index name in several
  headers, the first one; `headers` says how many.
- `summary`: the card or recipe summary, the compare title, or `null` for
  an index name.
- No results: exit 1 with `"results": []`.

## `cxxman -H <header> --json`

```json
{
  "schema": 1,
  "type": "header",
  "header": "algorithm",
  "summary": "Generic algorithms over ranges: search, sort, modify, partition.",
  "groups": [
    {"title": "Sort", "names": ["sort", "stable_sort", "partial_sort"], "since": null}
  ],
  "names": [
    {"name": "sort", "card": true, "kind": "algorithm", "since": "C++98"},
    {"name": "ranges::sort", "card": true, "kind": "algorithm", "since": "C++20"}
  ]
}
```

- `summary` and `groups` come from the header file; `null` and `[]` when
  there is none.
- `names` is every known name of the header once, sorted, including the
  `ranges::` names that the text output folds into one line.
- An unknown header: exit 1 with a `missing` document whose `name` is the
  header asked for and whose `suggestions` hold the closest known header,
  if any: `{"schema": 1, "type": "missing", "name": "algoritm",
  "suggestions": ["algorithm"]}`.
