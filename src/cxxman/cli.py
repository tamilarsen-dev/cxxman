"""Command-line interface for local C++ reference lookup and updates."""

from __future__ import annotations

import argparse
from difflib import get_close_matches
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
            "  cxxman list                List locally stored pages\n"
            "  cxxman topics              Show pages available to download\n"
            "  cxxman update [topic ...]  Download/update catalog pages"
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
        "action_or_topic",
        nargs="?",
        metavar="{topic,search,list,topics,update,check-updates,self-update}",
        help="topic to display, or a command",
    )
    parser.add_argument("arguments", nargs="*", metavar="ARG")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    """Parse arguments and dispatch a local read or explicit update."""
    parser = build_parser()
    args = parser.parse_args(argv)
    action = args.action_or_topic
    arguments = args.arguments

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
    if action == "update":
        return _update_pages(arguments)
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
    return _show_page(action, color=args.color)


def _show_page(
    topic: str, *, color: Literal["auto", "always", "never"] = "auto"
) -> int:
    try:
        with Database(default_database_path()) as database:
            document = database.get_document(topic)
    except CorruptDocumentError as error:
        print(f"cxxman: local page '{topic}' is corrupt: {error}", file=sys.stderr)
        return 1
    except (OSError, sqlite3.Error) as error:
        print(f"cxxman: could not open local documentation: {error}", file=sys.stderr)
        return 1
    if document is None:
        from cxxman.core.catalog import SOURCE_PAGES

        normalized_topic = topic.strip().casefold()
        if normalized_topic.startswith("std::"):
            normalized_topic = normalized_topic.removeprefix("std::")
        if normalized_topic in SOURCE_PAGES:
            print(
                f"cxxman: '{topic}' is not downloaded yet. Run 'cxxman update {topic}' to get it.",
                file=sys.stderr,
            )
        else:
            suggestions = get_close_matches(normalized_topic, SOURCE_PAGES, n=1, cutoff=0.6)
            if suggestions:
                print(
                    f"cxxman: unknown topic '{topic}'. Did you mean '{suggestions[0]}'? "
                    "Run 'cxxman topics' to see supported pages.",
                    file=sys.stderr,
                )
            else:
                print(
                    f"cxxman: unknown topic '{topic}'. Run 'cxxman topics' to see supported pages.",
                    file=sys.stderr,
                )
        return 1

    from cxxman.core.manual import project_document
    from cxxman.ui.renderer import create_console, render_manual_page

    render_manual_page(
        project_document(document),
        console=create_console(color),
        paginate=sys.stdout.isatty(),
    )
    return 0


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
        print(f"  {result.canonical_topic}  {result.title}")
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


def _update_pages(topics: list[str]) -> int:
    from cxxman.core.update import UpdateError, update_documents

    try:
        with Database(default_database_path()) as database:
            documents = update_documents(
                database,
                topics,
                on_page_start=lambda page: print(
                    f"Downloading {page.topic} from cppreference...", file=sys.stderr
                ),
            )
    except (UpdateError, OSError, sqlite3.Error) as error:
        print(f"cxxman: update failed: {error}", file=sys.stderr)
        return 1
    updated_topics = ", ".join(document.canonical_topic for document in documents)
    print(f"Updated {len(documents)} page(s): {updated_topics}")
    print(f"Open a page with: cxxman {documents[0].canonical_topic}")
    return 0


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
