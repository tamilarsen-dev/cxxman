# cxxman

A terminal-based C++ documentation viewer inspired by Linux `man`.
Read C++ library references, search cached pages, and update documentation
from the terminal.

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

## Download

Standalone builds for Windows x64, Linux x64, Intel macOS, and Apple
Silicon macOS are available in release `v0.1.0`. Download the archive for
your platform from
[GitHub Releases](https://github.com/tamilarsen-dev/cxxman/releases/latest)
and follow the [installation guide](docs/release.md).

The executable does not bundle cppreference pages. Run `cxxman update`
after installing to download the supported starter pages; reading and
searching cached pages works offline.

## Documentation

-   [User manual](docs/user-manual.md)
-   [Developer guide](docs/developer-guide.md)
-   [Cross-platform release and installation](docs/release.md)
-   [Release readiness](docs/release-readiness.md)
-   [MVP scope](docs/mvp.md)
-   [Architecture](docs/architecture.md)
-   [Performance strategy](docs/performance.md)
-   [Technology decisions](docs/technology-stack.md)
-   [Project structure](docs/project-structure.md)
-   [Development roadmap](docs/roadmap.md)

## License

This project is licensed under the [MIT License](LICENSE).

## Current status

The CLI currently supports `vector`, `vector::push_back`, `string`, and
`sort`. The public source repository is [hosted on GitHub](https://github.com/tamilarsen-dev/cxxman).
Windows x64, Linux x64, Intel macOS, and Apple Silicon macOS builds have
passed the [GitHub Actions release workflow](https://github.com/tamilarsen-dev/cxxman/actions).
The Windows executable has also been built and smoke-tested locally. The
first downloadable GitHub Release, [`v0.1.0`](https://github.com/tamilarsen-dev/cxxman/releases/tag/v0.1.0),
is published with four platform archives and SHA-256 checksums.

See [release readiness](docs/release-readiness.md) for the completed
publishing steps and platform notes.
