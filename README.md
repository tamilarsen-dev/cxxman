# cxxman

A terminal-based C++ documentation viewer inspired by Linux `man`.

`cxxman` is designed around a **local-first** workflow: documentation is
downloaded and parsed only when you explicitly update it, then read from
local storage when you open a page.

## Project goals

-   Display C++ reference documentation directly in the terminal.
-   Work offline after documentation has been downloaded.
-   Provide readable headings, paragraphs, lists, tables, signatures,
    and code examples.
-   Search documentation by symbol and topic.
-   Keep page lookup fast and memory usage bounded.
-   Provide native terminal builds for Windows, Linux, and macOS, while
    keeping the Python package portable.

## Quick vision

``` powershell
cxxman topics
cxxman update vector
cxxman vector
cxxman search vector
cxxman vector::push_back
```

## Documentation

-   [User manual](docs/user-manual.md)
-   [Developer guide](docs/developer-guide.md)
-   [Cross-platform release and installation](docs/release.md)
-   [MVP scope](docs/mvp.md)
-   [Architecture](docs/architecture.md)
-   [Performance strategy](docs/performance.md)
-   [Technology decisions](docs/technology-stack.md)
-   [Project structure](docs/project-structure.md)
-   [Development roadmap](docs/roadmap.md)

## Current status

Phases 1 through 7 are complete. The first focused library-function
manual is `std::vector::push_back`; the class overview pages remain
available when broader context is useful. First-run guidance and topic
discovery are included in the CLI.

A cross-platform standalone release workflow and per-user installers are
now prepared for Windows x64, Linux x64, and Intel and Apple Silicon Macs.
No GitHub Release has been published yet; see the
[release and installation guide](docs/release.md) for the install flow and
maintainer steps.
