# cxxman User Manual

`cxxman` is a terminal reference viewer for C++ standard-library pages. It
downloads pages only when you explicitly request an update. Viewing,
searching, and listing pages use the local database and work offline.

## Current scope

The published `v0.2.0` standalone release contains the 50-page catalog
available when it was built. The current source tree has a generated
catalog of 2,496 names across 111 headers, mapping to 2,167 unique source
routes in the supported cppreference `/cpp/` scope. Run `cxxman topics` to
see the catalog available in the version you are using. The catalog is not
the entirety of cppreference: it excludes C-language pages, C++
language-reference pages, experimental/TS routes, compiler-support tables,
and pure library-overview pages. See
[`project-history-and-features.md`](project-history-and-features.md) for the
full history and release/source distinction.

The original 50-page release catalog included these representative topics:

- **Containers, strings, and views:** `vector`, `vector::push_back`,
  `string`, `array`, `deque`, `forward_list`, `list`, `map`, `multimap`,
  `multiset`, `set`, `unordered_map`, `unordered_multimap`,
  `unordered_multiset`, `unordered_set`, `stack`, `queue`, `priority_queue`,
  `span`, and `string_view`.
- **Algorithms:** `sort`, `unique`, `all_of`, `adjacent_find`, `equal`,
  `binary_search`, `copy`, `count`, `find`, `for_each`, `lower_bound`,
  `max_element`, `min_element`, `remove`, `replace`, `reverse`,
  `transform`, `fill`, `generate`, `accumulate`, and `merge`.
- **Utilities and memory:** `optional`, `bitset`, `any`, `variant`, `pair`,
  `tuple`, `expected`, `unique_ptr`, and `shared_ptr`.

Some cppreference pages cover related operations together. For example,
the `all_of` page also documents `any_of` and `none_of`; use `cxxman topics`
for the exact downloadable topic names.

## Install and start

For development, install the repository into its Python virtual
environment. From the project directory in PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned
.\.venv\Scripts\Activate.ps1
python -m pip install -e .
```

Standalone release `v0.2.0` is available for Windows x64, Linux x64,
Intel Macs, and Apple Silicon Macs. It includes the 50-page catalog and
Windows setup `.exe` with a generated uninstaller. See the [release
installation guide](release.md) for setup and app updates. For development,
install from the current repository using the steps below. Ask for
command help with:

```powershell
cxxman --help
cxxman --version
```

Running `cxxman` with no arguments prints a short quick start:

```powershell
cxxman
```

You can also run the package as a module during development:

```powershell
python -m cxxman --help
```

## Download pages

To see every topic the current version supports, run:

```powershell
cxxman topics
```

This is the downloadable catalog. `cxxman list` shows only pages already
stored in your local database.

Run an explicit update to fetch and store every page in the catalog
supported by the installed version. In the current source tree this is the
full generated catalog; the published `v0.2.0` executable has the earlier
50-page catalog:

```powershell
cxxman update
```

Or request one page:

```powershell
cxxman update vector
cxxman update vector::push_back
cxxman update string
cxxman update sort
cxxman update unique
cxxman update optional
```

To download only one page, pass its topic, for example
`cxxman update vector`.

The `std::` prefix is accepted too, for example
`cxxman update std::vector::push_back`. Updates require an internet
connection and retrieve documentation from cppreference. If a download or
parse fails, the update reports that page's error and continues. Successful
pages are committed individually, and a failed page does not replace its
previously stored version.

## Read a page

After downloading a topic, open it by its topic name:

```powershell
cxxman vector
cxxman vector::push_back
cxxman std::vector::push_back
```

The page is rendered as a concise manual-style view. Recognized source
sections are shown only when they contain content; declarations, lists,
tables, and references remain structured. The original parsed document is
still stored locally. Long pages open a pager when output is connected to
an interactive terminal. Piped output is printed directly without the
pager.

Code blocks use C++ syntax highlighting. Color follows terminal support by
default; select a mode with `--color auto`, `--color always`, or
`--color never` (for example, `cxxman vector --color never`).

Reading a page never downloads or parses HTML. If the page is not cached,
`cxxman` reports that and suggests `cxxman update`.

## Search cached pages

Search only checks pages already stored on this computer:

```powershell
cxxman search vector
cxxman search push_back
cxxman search sequence container
```

Search returns up to 20 matching page titles and topic names. When SQLite
FTS5 is available, query terms are combined and support token-prefix
matching. Each result includes a command you can run to open that page.
Search results are metadata only; open a result by its topic to read the
full page. If nothing matches, the message distinguishes an empty local
cache from a query with no matches.

## List cached pages

```powershell
cxxman list
```

This lists up to 100 locally available pages. If none have been downloaded,
the command tells you to run `cxxman update`.

## Local data and offline use

The database is stored per user:

- Windows: `%LOCALAPPDATA%\cxxman\cxxman.sqlite3`
- macOS and Linux: `$XDG_DATA_HOME/cxxman/cxxman.sqlite3`, or
  `~/.local/share/cxxman/cxxman.sqlite3` when `XDG_DATA_HOME` is unset.

To use a different database, set `CXXMAN_DB_PATH` before starting the
command. For example:

```powershell
$env:CXXMAN_DB_PATH = "$env:TEMP\cxxman.sqlite3"
cxxman list
```

After pages are downloaded, `cxxman <topic>`, `cxxman search <query>`,
and `cxxman list` need no network connection. `cxxman update` downloads
reference pages. `cxxman check-updates` checks for a newer application
release; `cxxman self-update` installs it for standalone builds. These
commands need a network connection. Python-managed installations should
use pip or reinstall from source. Page updates print a message before each
download and suggest a page command when they finish.

## Common problems

- **Topic not available locally:** run `cxxman update <topic>`.
- **Unsupported topic:** check the catalog with `cxxman topics`; arbitrary
  cppreference URLs are not accepted yet.
- **Update failed:** check internet access and retry. Previously stored
  valid pages are preserved when an update fails.
- **Application update:** run `cxxman check-updates`. For a standalone
  installation, run `cxxman self-update`; for a Python installation, use
  pip or reinstall from source.
- **Command not found:** activate the development virtual environment or
  check that its `Scripts` directory is on `PATH`. Standalone release
  archives are also available; see [release installation](release.md).
