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

## Features

- **Quick manual pages:** concise, source-backed cards show purpose,
  `#include`, declarations, and applicable behavior. Use `--full` for the
  complete stored document or `--example` for source examples.
- **Offline reads:** page viewing, search, listing, and header discovery use
  local SQLite data. HTML parsing and network access happen only during
  explicit updates.
- **Broad topic discovery in the current source:** a generated catalog with
  2,496 names across 111 headers, mapped to 2,167 distinct cppreference
  routes. Search ranks names, aliases, purpose text, and page content;
  `cxxman header <name>` lists entities by header.
- **Resilient updates:** targeted and full-catalog updates, per-page commits,
  retry/backoff, failure reporting, and `cxxman update --resume`.
- **Terminal support:** Rich rendering, C++ syntax highlighting,
  `--color auto|always|never`, terminal-width-aware output, paging for long
  pages, and quiet handling of closed output pipes.
- **Cross-platform standalone app:** Windows x64, Linux x64, Intel macOS,
  and Apple Silicon packages; Windows per-user installer; checksums; and
  application check/self-update commands.

## Download

Standalone builds for Windows x64, Linux x64, Intel macOS, and Apple
Silicon macOS are available in release `v0.2.0`. Download the archive for
your platform from
[GitHub Releases](https://github.com/tamilarsen-dev/cxxman/releases/latest)
and follow the [installation guide](docs/release.md).

Release `v0.2.0` includes a Windows per-user setup `.exe` with a generated
uninstaller. Standalone builds can check for and install app releases with
`cxxman check-updates` and `cxxman self-update`.

The executable does not bundle cppreference pages. The published `v0.2.0`
build contains the 50-page catalog available at release time. The current
source tree has a generated catalog of 2,496 names across 111 headers
(2,167 distinct source routes); run `cxxman update` from that source version
to fetch the full catalog, or use `cxxman update <topic>` for selected pages.
Reading and searching cached pages work offline.

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
-   [Project history and complete feature reference](docs/project-history-and-features.md)

## License

This project is licensed under the [MIT License](LICENSE).

## Current status

The current source catalog contains 2,496 named entries across 111 headers,
mapping to 2,167 distinct cppreference routes in the supported `/cpp/` scope.
The latest local coverage sweep found every route stored and no page/category
findings; four entries have no purpose sentence in their source and show only
their title. This is structural coverage, not a manual audit of every
technical fact. The published `v0.2.0` binaries still include the older
50-page catalog; the full-catalog source work has not been released yet. See
[`docs/coverage-report.md`](docs/coverage-report.md) and
[`cxxman topics`](docs/user-manual.md#download-pages) for details. The public
source repository is
[hosted on GitHub](https://github.com/tamilarsen-dev/cxxman).
Windows x64, Linux x64, Intel macOS, and Apple Silicon macOS builds have
passed the [GitHub Actions release workflow](https://github.com/tamilarsen-dev/cxxman/actions).
GitHub Release [`v0.2.0`](https://github.com/tamilarsen-dev/cxxman/releases/tag/v0.2.0)
provides four platform builds, a Windows setup executable, and SHA-256
checksums.

See [release readiness](docs/release-readiness.md) for the completed
publishing steps and platform notes. See the
[project history and feature reference](docs/project-history-and-features.md)
for the full timeline, current source/release distinction, feature inventory,
and documentation index.
