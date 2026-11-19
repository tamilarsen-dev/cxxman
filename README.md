# cxxman

A terminal-based C++ documentation viewer inspired by Linux `man`.

`cxxman` is designed around a **local-first** workflow: documentation is
downloaded and parsed during installation or updates, then read from
local storage when the user opens a page.

## Project goals

-   Display C++ reference documentation directly in the terminal.
-   Work offline after documentation has been downloaded.
-   Provide readable headings, paragraphs, lists, tables, signatures,
    and code examples.
-   Search documentation by symbol and topic.
-   Keep page lookup fast and memory usage bounded.
-   Support Windows PowerShell first, while keeping the Python package
    portable.

## Quick vision

``` powershell
cxxman vector
cxxman string
cxxman sort
cxxman search unordered_map
```

## Documentation

-   [MVP scope](docs/mvp.md)
-   [Architecture](docs/architecture.md)
-   [Performance strategy](docs/performance.md)
-   [Technology decisions](docs/technology-stack.md)
-   [Project structure](docs/project-structure.md)
-   [Development roadmap](docs/roadmap.md)

## Current status

Early project setup. The first milestone is a working CLI skeleton.
Documentation ingestion, local storage, search, and rendering are
subsequent milestones.
