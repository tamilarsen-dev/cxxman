# cxxman User Manual

`cxxman` is a terminal reference viewer for a small, curated set of C++
library pages. It downloads pages only when you explicitly request an
update. Viewing, searching, and listing pages use the local database and
work offline.

## Current scope

The current starter catalog contains these pages:

| Topic | What it covers | Header |
|---|---|---|
| `vector` | The `std::vector` container reference | `<vector>` |
| `vector::push_back` | The focused `std::vector::push_back` manual | `<vector>` |
| `string` | The `std::basic_string` / `std::string` reference | `<string>` |
| `sort` | The `std::sort` algorithm reference | `<algorithm>` |

This is a curated starter set, not a complete copy of cppreference. A
topic must be in the catalog to be downloaded.

## Install and start

The current development install uses the repository's Python virtual
environment. From the project directory in PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned
.\.venv\Scripts\Activate.ps1
python -m pip install -e .
```

Standalone GitHub Releases are a planned distribution format and are not
available yet. Once installed, ask for command help with:

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

Run an explicit update to fetch and store the full current starter set:

```powershell
cxxman update
```

Or request one page:

```powershell
cxxman update vector
cxxman update vector::push_back
cxxman update string
cxxman update sort
```

The `std::` prefix is accepted too, for example
`cxxman update std::vector::push_back`. Updates require an internet
connection and retrieve documentation from cppreference. If a download or
parse fails, the update reports an error; database writes are committed
atomically so a failed batch does not partially replace cached pages.

## Read a page

After downloading a topic, open it by its topic name:

```powershell
cxxman vector
cxxman vector::push_back
cxxman std::vector::push_back
```

The page is rendered in the terminal with headings, paragraphs, code and
signatures, lists, tables, and references where the source page provides
them. Long pages open a pager when output is connected to an interactive
terminal. Piped output is printed directly without the pager.

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
and `cxxman list` need no network connection. `cxxman update` is the only
current network-enabled command. Updates print a message before each page
download and suggest a page command when they finish.

## Common problems

- **Topic not available locally:** run `cxxman update <topic>`.
- **Unsupported topic:** check the starter catalog above; arbitrary
  cppreference URLs are not accepted yet.
- **Update failed:** check internet access and retry. Previously stored
  valid pages are preserved when an update fails.
- **Command not found:** activate the development virtual environment or
  check that its `Scripts` directory is on `PATH`. The planned standalone
  release should remove this Python environment setup step.
