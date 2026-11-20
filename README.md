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
Silicon macOS are available in release `v0.2.0`. Download the archive for
your platform from
[GitHub Releases](https://github.com/tamilarsen-dev/cxxman/releases/latest)
and follow the [installation guide](docs/release.md).

Release `v0.2.0` includes a Windows per-user setup `.exe` with a generated
uninstaller. Standalone builds can check for and install app releases with
`cxxman check-updates` and `cxxman self-update`.

The executable does not bundle cppreference pages. Run `cxxman update`
after installing to download all 50 catalog pages; use
`cxxman update <topic>` to fetch only one. Reading and searching cached
pages work offline.

Manual pages use C++ syntax highlighting and terminal colors by default.
Choose `--color auto`, `--color always`, or `--color never` when displaying
a page.

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

The current source catalog contains 50 C++ standard-library pages across
containers, algorithms, utilities, and memory. See `cxxman topics` for the
current list. The public source repository is
[hosted on GitHub](https://github.com/tamilarsen-dev/cxxman).
Windows x64, Linux x64, Intel macOS, and Apple Silicon macOS builds have
passed the [GitHub Actions release workflow](https://github.com/tamilarsen-dev/cxxman/actions).
GitHub Release [`v0.2.0`](https://github.com/tamilarsen-dev/cxxman/releases/tag/v0.2.0)
provides four platform builds, a Windows setup executable, and SHA-256
checksums.

See [release readiness](docs/release-readiness.md) for the completed
publishing steps and platform notes.
