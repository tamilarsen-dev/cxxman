# Performance Strategy

## 1. Principle: local-first

The ordinary command:

``` powershell
cxxman vector
```

must read from local storage. It must not make a network request.

``` text
User command
    |
    v
Local documentation cache
    |  Read only the requested page
    v
Parser / Document model
    |  Already structured; no HTML parsing at view time
    v
Terminal renderer
    |
    v
Formatted terminal output
```

The parser is part of ingestion/update, not the page-viewing hot path.

## 2. Rules

1.  No HTTP request when displaying a page or searching.
2.  Parse HTML only when downloading or updating documentation.
3.  Persist parsed content locally.
4.  Use SQLite for metadata and search indexing.
5.  Do not load the entire documentation corpus into RAM.
6.  Load only the requested page and the search results needed for the
    current operation.
7.  Use pagination for long output.
8.  Keep startup work minimal: avoid scanning the whole corpus or
    initializing expensive components unnecessarily.
9.  Add indexes for common lookup fields.
10. Measure performance before optimizing.

## 3. Storage approach

SQLite is the local index and metadata store. The initial implementation
should compare two practical page-storage options:

-   Store structured document JSON in SQLite rows.
-   Store structured page files on disk and keep metadata/search text in
    SQLite.

Choose the simpler approach for the MVP based on measured page size,
read latency, update complexity, and integrity requirements. Do not
introduce a second storage mechanism without a demonstrated benefit.

SQLite FTS5 can be evaluated for full-text search. Keep exact symbol
lookup indexed separately.

## 4. Lazy loading

-   Resolve a topic to a page ID using indexed metadata.
-   Read only that page's structured content.
-   Render blocks incrementally where practical.
-   Search should return a bounded number of results rather than
    materializing the full corpus.

## 5. Performance measurements

Measure at least:

-   Cold CLI startup.
-   Warm CLI startup.
-   Local page lookup and load.
-   Rendering a typical page.
-   Search latency for a small and larger corpus.
-   Peak memory for page viewing.

Use `timeit` for focused Python operations and `cProfile` for locating
expensive code paths. Record corpus size and machine details alongside
measurements.

## 6. Performance goals

Treat these as engineering targets to validate, not guarantees:

-   Cached page viewing should feel immediate on a normal SSD.
-   Search should use SQLite indexes rather than scanning every page in
    Python.
-   Memory use should scale with the requested page and result set, not
    the total corpus.
-   Network speed must not affect normal page viewing.

Avoid premature micro-optimization. The largest performance win is
removing network access and HTML parsing from the read path.

## 7. Phase 7 baseline (2026-11-19)

Measurements were collected with `tools/benchmark.py` on Windows 11
(build 26200), Python 3.15.2, SQLite 3.53.4 with FTS5 enabled, and an
Intel 64-bit processor (Family 6, Model 126, Stepping 5; 8 logical CPUs).
The harness creates temporary synthetic
corpora of 3 and 1,000 pages. Each synthetic page has six typed blocks;
the 1,000-page corpus contains 453,000 bytes of serialized page blocks.
These pages are representative test data, not downloaded cppreference
content.

`timeit.repeat` used five repeats of 500 operations for lookup and search,
and five repeats of 100 operations for rendering. Process startup was
measured over six subprocess runs of `python -m cxxman search sequence`
against the 1,000-page database. The first process took 239 ms; the
median of the next five was 255 ms. This is process startup with local
database initialization and search, not a cold operating-system cache
measurement.

| Corpus | SQLite file | Exact lookup/load | FTS5 search |
|---|---:|---:|---:|
| 3 synthetic pages | 40 KiB | 0.166 ms | 0.244 ms |
| 1,000 synthetic pages | 1.37 MiB | 0.370 ms | 1.645 ms |

For the 1,000-page corpus, rendering one six-block page took 3.248 ms.
`tracemalloc` reported a 4,301-byte Python allocation peak while loading
one page. This excludes native SQLite allocations and process memory.
The SQLite content JSON averaged 453 bytes per page. The database stores
the FTS index and metadata as well, so its full file size is not directly
comparable to that content size.

`cProfile` over 100 large-corpus search calls attributed 0.242 seconds to
SQLite `execute`, out of 0.262 seconds total. The timed query returned at
most 20 metadata results.

These measurements establish a reproducible baseline, not a performance
claim for real cppreference pages. They do not compare JSON files against
SQLite using a real corpus, and they do not capture native memory. Repeat
the measurements after ingesting real pages before changing the storage
format or claiming a target is met.
