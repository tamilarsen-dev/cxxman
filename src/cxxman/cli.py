"""Command-line interface for local C++ reference lookup and updates."""

from __future__ import annotations

import argparse
from difflib import get_close_matches
import os
import sqlite3
import sys
from collections.abc import Sequence
from pathlib import Path
from typing import Literal

from cxxman import __version__
from cxxman.storage.database import (
    CorruptDocumentError,
    Database,
    default_database_path,
)


def build_parser() -> argparse.ArgumentParser:
    """Create the command parser while preserving ``cxxman <topic>``."""
    parser = argparse.ArgumentParser(
        prog="cxxman",
        description="Read C++ reference documentation in your terminal.",
        epilog=(
            "Commands:\n"
            "  cxxman <topic>             Show one locally stored page\n"
            "  cxxman search <query>      Search local documentation\n"
            "  cxxman header <name>       List entities in a standard header\n"
            "  cxxman list                List locally stored pages\n"
            "  cxxman topics              Show pages available to download\n"
            "  cxxman update [topic ...]  Download/update catalog pages"
            "\n  cxxman update --resume     Continue past pages already cached"
            "\n"
            "  cxxman check-updates       Check for a newer cxxman release\n"
            "  cxxman self-update         Download and install a newer release"
            "\n\nExamples:\n"
            "  cxxman topics\n"
            "  cxxman update vector::push_back\n"
            "  cxxman vector::push_back\n"
            "  cxxman search push_back"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--version",
        action="version",
        version=f"%(prog)s {__version__}",
    )
    parser.add_argument(
        "--color",
        choices=("auto", "always", "never"),
        default="auto",
        metavar="WHEN",
        help="colorize manual pages: auto follows the terminal (default), always, or never",
    )
    parser.add_argument(
        "--resume",
        action="store_true",
        help="skip locally stored pages during a full catalog update",
    )
    display_mode = parser.add_mutually_exclusive_group()
    display_mode.add_argument(
        "--full", action="store_true", help="show the complete stored reference document"
    )
    display_mode.add_argument(
        "--example", action="store_true", help="show the complete source example section"
    )
    parser.add_argument(
        "action_or_topic",
        nargs="?",
        metavar="{topic,search,header,list,topics,update,check-updates,self-update}",
        help="topic to display, or a command",
    )
    parser.add_argument("arguments", nargs="*", metavar="ARG")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    """Dispatch a command and quietly handle a consumer that closes its pipe."""
    try:
        exit_code = _main(argv)
        sys.stdout.flush()
        return exit_code
    except OSError as error:
        if not isinstance(error, BrokenPipeError) and error.errno != 22:
            raise
        try:
            sys.stdout.close()
        except OSError:
            pass
        sys.stdout = open(os.devnull, "w", encoding="utf-8")
        return 0


def _main(argv: Sequence[str] | None = None) -> int:
    """Parse arguments and dispatch a local read or explicit update."""
    parser = build_parser()
    args = parser.parse_args(argv)
    action = args.action_or_topic
    arguments = args.arguments
    if args.resume and action != "update":
        parser.error("--resume applies only to update")
    if (args.full or args.example) and action in {
        None,
        "search",
        "list",
        "topics",
        "header",
        "update",
        "check-updates",
        "self-update",
    }:
        parser.error("--full and --example apply only to a topic")

    if action is None:
        print("cxxman — C++ reference pages in your terminal")
        print("Quick start:")
        print("  cxxman topics                 See available pages")
        print("  cxxman update vector          Download a page")
        print("  cxxman vector                 Read it offline")
        print("\nRun 'cxxman --help' to see all commands.")
        return 0
    if action == "search":
        if not arguments:
            parser.error("search requires a query")
        return _search(" ".join(arguments))
    if action == "list":
        if arguments:
            parser.error("list does not accept arguments")
        return _list_pages()
    if action == "topics":
        if arguments:
            parser.error("topics does not accept arguments")
        return _list_supported_topics()
    if action == "header":
        if len(arguments) != 1:
            parser.error("header requires exactly one header name")
        return _list_header(arguments[0])
    if action == "update":
        return _update_pages(arguments, resume=args.resume)
    if action == "check-updates":
        if arguments:
            parser.error("check-updates does not accept arguments")
        return _check_app_updates()
    if action == "self-update":
        if arguments:
            parser.error("self-update does not accept arguments")
        return _self_update()
    if arguments:
        parser.error("a topic command accepts one topic")
    if action.startswith("<") and action.endswith(">"):
        return _list_header(action)
    normalized_action = action.strip().casefold()
    if normalized_action.startswith("std::"):
        normalized_action = normalized_action[5:]
    if _find_source_page(normalized_action) is None and _matching_header(normalized_action):
        return _list_header(normalized_action)
    mode = "full" if args.full else ("example" if args.example else "quick")
    return _show_page(action, color=args.color, mode=mode)


def _show_page(
    topic: str,
    *,
    color: Literal["auto", "always", "never"] = "auto",
    mode: Literal["quick", "full", "example"] = "quick",
) -> int:
    try:
        with Database(default_database_path()) as database:
            document = database.get_document(topic)
            needs_update = document is not None and database.document_needs_update(topic)
    except CorruptDocumentError as error:
        print(f"cxxman: local page '{topic}' is corrupt: {error}", file=sys.stderr)
        return 1
    except (OSError, sqlite3.Error) as error:
        print(f"cxxman: could not open local documentation: {error}", file=sys.stderr)
        return 1
    if document is None:
        page = _find_source_page(topic)

        normalized_topic = topic.strip().casefold()
        if normalized_topic.startswith("std::"):
            normalized_topic = normalized_topic.removeprefix("std::")
        if page is not None:
            print(
                f"cxxman: '{topic}' is not downloaded yet. "
                f"Run 'cxxman update {page.topic}' to get it.",
                file=sys.stderr,
            )
        else:
            suggestions = _topic_suggestions(normalized_topic)
            header = _matching_header(normalized_topic)
            if suggestions or header:
                choices = [f"'{name}' (cxxman {name})" for name in suggestions]
                if header:
                    choices.append(f"header <{header}>")
                print(
                    f"cxxman: unknown topic '{topic}'. Suggestions: "
                    + "; ".join(choices[:4])
                    + ". Browse: cxxman topics.",
                    file=sys.stderr,
                )
            else:
                print(
                    f"cxxman: unknown topic '{topic}'. Run 'cxxman topics' to see supported pages.",
                    file=sys.stderr,
                )
        return 1

    from cxxman.core.manual import extract_example_document, project_document
    from cxxman.ui.renderer import create_console, render_document, render_manual_page

    if needs_update:
        print(
            f"cxxman: '{topic}' was parsed by an older version. "
            f"Run 'cxxman update {topic}' to refresh it.",
            file=sys.stderr,
        )
    console = create_console(color)
    if mode == "full":
        render_document(document, console=console, paginate=sys.stdout.isatty())
    elif mode == "example":
        example_document = extract_example_document(document)
        if example_document is None:
            print(f"No source example is stored for '{topic}'.", file=sys.stderr)
            return 1
        render_document(example_document, console=console, paginate=sys.stdout.isatty())
    else:
        render_manual_page(
            project_document(document),
            console=console,
            paginate=sys.stdout.isatty(),
        )
    return 0


def _find_source_page(topic: str):
    from cxxman.core.catalog import SOURCE_PAGES

    normalized = topic.strip().casefold()
    if normalized.startswith("std::"):
        normalized = normalized.removeprefix("std::")
    for page in SOURCE_PAGES.values():
        if normalized == page.topic.casefold() or normalized in {
            alias.casefold() for alias in page.aliases
        }:
            return page
    return None


def _topic_suggestions(topic: str) -> list[str]:
    from cxxman.core.catalog import SOURCE_PAGES

    name_to_topic: dict[str, str] = {}
    for page in SOURCE_PAGES.values():
        name_to_topic[page.topic.casefold()] = page.topic
        for alias in page.aliases:
            name_to_topic[alias.casefold()] = page.topic
    matches = get_close_matches(topic, name_to_topic, n=6, cutoff=0.55)
    suggestions: list[str] = []
    for match in matches:
        canonical_topic = name_to_topic[match]
        if canonical_topic not in suggestions:
            suggestions.append(canonical_topic)
        if len(suggestions) == 3:
            break
    return suggestions


def _matching_header(topic: str) -> str | None:
    from cxxman.core.catalog import SOURCE_PAGES

    normalized = topic.strip().casefold().strip("<>")
    headers = {
        page.header[1:-1].casefold()
        for page in SOURCE_PAGES.values()
        if page.header and page.header.startswith("<") and page.header.endswith(">")
    }
    return normalized if normalized in headers else None


def _search(query: str) -> int:
    try:
        with Database(default_database_path()) as database:
            results = database.search(query, limit=20)
            cached_pages = database.list_topics(limit=1)
    except (OSError, sqlite3.Error) as error:
        print(f"cxxman: could not search local documentation: {error}", file=sys.stderr)
        return 1
    if not results:
        if not cached_pages:
            print("No pages are cached yet. Run 'cxxman topics', then 'cxxman update <topic>'.")
        else:
            print(f"No local results for '{query}'. Search only checks pages you've downloaded.")
        return 0
    print(f"Found {len(results)} local page(s):")
    for result in results:
        details = [result.canonical_topic]
        if result.header:
            details.append(result.header)
        details.append(result.purpose or result.title)
        print("  " + "  ".join(details))
        print(f"    Open with: cxxman {result.canonical_topic}")
    return 0


def _list_pages() -> int:
    try:
        with Database(default_database_path()) as database:
            topics = database.list_topics(limit=100)
    except (OSError, sqlite3.Error) as error:
        print(f"cxxman: could not list local documentation: {error}", file=sys.stderr)
        return 1
    if not topics:
        print("No pages are cached yet. Run 'cxxman topics', then 'cxxman update <topic>'.")
        return 0
    print(f"{len(topics)} cached page(s):")
    for topic, title in topics:
        print(f"  {topic}  {title}")
    return 0


def _list_supported_topics() -> int:
    from cxxman.core.catalog import SOURCE_PAGES

    print("Topics available to download (run 'cxxman update <topic>'):")
    for page in SOURCE_PAGES.values():
        print(f"  {page.topic:<22} {page.header or ''}")
    return 0


def _list_header(header: str) -> int:
    from cxxman.core.catalog import pages_for_header

    pages = pages_for_header(header)
    if not pages:
        normalized = header.strip().casefold().strip("<>")
        print(f"cxxman: no catalog entries for header <{normalized}>.", file=sys.stderr)
        return 1

    print(f"<{header.strip().strip('<>')}> — {len(pages)} entries")
    current_group: str | None = None
    for page in pages:
        if page.group and page.group != current_group:
            current_group = page.group
            print(f"\n{current_group}")
        names = page.topic
        if page.aliases:
            names += ", " + ", ".join(page.aliases)
        print(f"  {names}")
        if page.purpose:
            print(f"    {page.purpose}")
    return 0


def _update_pages(topics: list[str], *, resume: bool = False) -> int:
    from cxxman.core.update import UpdateError, update_documents

    try:
        with Database(default_database_path()) as database:
            report = update_documents(
                database,
                topics,
                resume=resume,
                on_page_start=lambda page: print(
                    f"Downloading {page.topic} from cppreference...", file=sys.stderr
                ),
            )
    except (UpdateError, OSError, sqlite3.Error) as error:
        print(f"cxxman: update failed: {error}", file=sys.stderr)
        return 1
    print(
        f"Update complete: {len(report.updated)} updated, "
        f"{len(report.failures)} failed, {len(report.skipped)} skipped."
    )
    if report.updated:
        print(f"Open a page with: cxxman {report.updated[0].canonical_topic}")
    for failure in report.failures:
        print(f"Failed {failure.topic}: {failure.reason}", file=sys.stderr)
    return int(bool(report.failures))


def _check_app_updates() -> int:
    from cxxman.core.app_updates import (
        AppUpdateError,
        fetch_latest_release,
        is_update_available,
    )

    try:
        release = fetch_latest_release()
    except AppUpdateError as error:
        print(f"cxxman: update check failed: {error}", file=sys.stderr)
        return 1
    if is_update_available(__version__, release.version):
        print(f"Update available: cxxman {release.version} (current: {__version__})")
        print(f"Release notes: {release.url}")
        print("Run 'cxxman self-update' to install it.")
    else:
        print(f"cxxman is up to date ({__version__}).")
    return 0


def _self_update() -> int:
    from cxxman.core.app_updates import (
        AppUpdateError,
        fetch_latest_release,
        install_release_update,
        is_update_available,
    )

    if not getattr(sys, "frozen", False):
        print(
            "This installation is managed by Python. Update it with pip or reinstall from source.",
            file=sys.stderr,
        )
        return 1
    try:
        release = fetch_latest_release()
        if not is_update_available(__version__, release.version):
            print(f"cxxman is already up to date ({__version__}).")
            return 0
        message = install_release_update(release, executable_path=Path(sys.executable))
    except AppUpdateError as error:
        print(f"cxxman: self-update failed: {error}", file=sys.stderr)
        return 1
    print(message)
    return 0
