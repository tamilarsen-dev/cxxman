# MVP Definition

## 1. MVP objective

Deliver a usable terminal application that can display a small, locally
stored set of C++ reference pages with readable structure and
predictable performance.

The MVP is not a full mirror of cppreference. It proves the complete
path from a command to a locally stored document and a readable terminal
page.

## 2. Primary user stories

1.  As a user, I can run `cxxman vector` and read the `std::vector`
    reference page in my terminal.
2.  As a user, I can run `cxxman search vector` and find matching local
    documentation entries.
3.  As a user, I can see whether a page is available locally.
4.  As a user, I can download or update documentation explicitly, rather
    than triggering network access during ordinary page viewing.
5.  As a user, I can navigate long output using terminal pagination.

## 3. MVP commands

  ------------------------------------------------------------------------
  Command                  Purpose                 Network access
  ------------------------ ----------------------- -----------------------
  `cxxman vector`          Display a locally       Never
                           cached page             

  `cxxman search vector`   Search the local index  Never

  `cxxman list`            List locally available  Never
                           pages                   

  `cxxman update`          Download and parse the  Yes
                           selected documentation  
                           corpus                  

  `cxxman --version`       Show application        Never
                           version                 

  `cxxman --help`          Show command help       Never
  ------------------------------------------------------------------------

The initial implementation may use a small curated page set. A later
release can add broader corpus management and version selection.

## 4. MVP document content

The parser and document model should preserve, where present:

-   Page title and canonical symbol.
-   Header name, such as `<vector>`.
-   Section headings.
-   Paragraphs.
-   Ordered and unordered lists.
-   Function signatures and code examples.
-   Tables, including complexity and member-function tables.
-   Links or references to related pages, at least as text in the MVP.

Unsupported or unusually complex HTML elements should degrade
gracefully. The renderer must not flatten the entire page into one
paragraph.

## 5. MVP non-goals

Not required for the first usable release:

-   Full offline mirror of every C++ standard and every cppreference
    page.
-   JavaScript execution or browser rendering.
-   GUI.
-   Automatic background updates.
-   Multiple documentation providers.
-   Advanced fuzzy ranking, autocomplete, or interactive cross-reference
    navigation.
-   Perfect reproduction of every HTML table layout.

## 6. Acceptance criteria

The MVP is complete when:

-   `cxxman --help` and `cxxman --version` work.
-   `cxxman vector` displays a page already stored locally.
-   Opening a page performs no HTTP request.
-   The page has distinct headings, paragraphs, lists, and
    code/signature blocks.
-   `cxxman search vector` queries SQLite and returns relevant local
    entries.
-   Only the requested page is loaded for display; the full corpus is
    not loaded into memory.
-   Long output can be paged.
-   Parser, storage, search, and CLI behavior have automated tests.
-   A missing page produces a useful message and does not silently
    access the network.

## 7. MVP delivery sequence

1.  CLI skeleton and package entry point.
2.  Document model.
3.  HTML parser using saved HTML fixtures.
4.  Terminal renderer.
5.  SQLite schema and repository layer.
6.  Local page lookup and search.
7.  Explicit documentation download/update command.
8.  Pagination, error handling, tests, and performance measurements.
