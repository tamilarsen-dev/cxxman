# Architecture

## 1. High-level flow

Ordinary page viewing is a local read path:

``` text
User command
    |
    v
CLI / argument parsing
    |
    v
Local page lookup
    |
    v
SQLite metadata + page storage
    |
    v
Document model
    |
    v
Manual-page projection
    |
    v
Terminal renderer
    |
    v
Terminal output
```

Documentation ingestion is a separate write path:

``` text
Explicit update command
    |
    v
HTTP client
    |
    v
Downloaded HTML
    |
    v
HTML parser
    |
    v
Structured document model
    |
    v
SQLite / local document storage
```

The two paths must remain separate. A normal `show` operation must never
download or parse remote HTML.

Application updates use a separate explicit path: `check-updates` reads
the GitHub release API, and `self-update` downloads the platform asset,
verifies its SHA-256 sidecar, then starts the Windows setup program or
replaces the standalone POSIX executable. This path is not used by page
viewing, search, or listing.

## 2. Component responsibilities

### CLI (`cli.py`)

-   Parse commands and options using `argparse`.
-   Validate user input.
-   Call application services.
-   Convert expected application errors into concise terminal messages.
-   Keep business logic out of argument parsing.

### Core (`core/`)

-   `document.py`: define the structured document model.
-   `manual.py`: project a loaded generic document into a concise page
    using recognized source headings and known page identity. Unrecognized
    facts are not inferred.
-   `search.py`: define search behavior and ranking over local indexed
    data.
-   `update.py`: fetch and parse only when the user invokes `update`, then
    atomically commit the selected pages.
-   `catalog.py`: define the 50 curated source pages, their stable URLs,
    headers, and entity kinds. A no-topic update selects the full catalog.

### Parser (`parser/`)

-   `html_parser.py`: transform downloaded cppreference HTML into the
    document model.
-   Parsing occurs during ingestion or update, not during page viewing.

### Storage (`storage/`)

-   `database.py`: manage SQLite connections, schema, transactions, and
    queries.
-   Store page metadata and searchable text.
-   Retrieve one requested page at a time.

### UI (`ui/`)

-   `renderer.py`: render generic documents or projected manual pages using Rich.
-   Highlight C++ code, handle headings, paragraphs, code blocks, tables,
    and pagination. Manual pages support automatic, forced, or disabled
    terminal color.
-   Keep rendering independent from HTTP and database implementation
    details.

## 3. Important boundaries

-   CLI depends on application/core interfaces, not on HTML parsing
    internals.
-   Parser produces a document model; it does not print to the terminal.
-   Renderer consumes prepared document/manual-page models; it does not
    fetch pages or extract source semantics.
-   Storage persists structured data; it does not parse HTML.
-   HTTP is used only by explicit ingestion/update workflows.

## 4. Data model direction

A document should contain a stable identifier, title, canonical symbol,
source URL, header (when applicable), and an ordered sequence of content
blocks.

Possible block types:

-   Heading
-   Paragraph
-   CodeBlock
-   List
-   Table
-   Link/reference

Use explicit typed structures rather than storing the whole page as a
single plain-text string.

## 5. Failure handling

-   Unknown topic: report that no matching page exists.
-   Page not downloaded: explain how to run the update/download command.
-   Corrupt local record: report the affected page and avoid crashing
    the entire CLI.
-   Network failure during update: preserve the previous valid local
    version.
-   Unsupported HTML element: preserve readable text where possible and
    record parser limitations in tests.
