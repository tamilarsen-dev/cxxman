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
