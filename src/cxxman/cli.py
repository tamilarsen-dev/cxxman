"""Command-line interface for local C++ reference lookup and updates."""

from __future__ import annotations

import argparse
import sqlite3
import sys
from collections.abc import Sequence

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
            "  cxxman update [topic ...]  Download/update starter pages"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--version",
        action="version",
        version=f"%(prog)s {__version__}",
    )
    parser.add_argument(
        "action_or_topic",
        nargs="?",
        metavar="{topic,search,list,update}",
        help="topic to display, or search/list/update command",
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
        parser.print_help()
        return 0
    if action == "search":
        if not arguments:
            parser.error("search requires a query")
        return _search(" ".join(arguments))
    if action == "list":
        if arguments:
            parser.error("list does not accept arguments")
        return _list_pages()
    if action == "update":
        return _update_pages(arguments)
    if arguments:
        parser.error("a topic command accepts one topic")
    return _show_page(action)


def _show_page(topic: str) -> int:
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
        print(
            f"cxxman: '{topic}' is not available locally. Run 'cxxman update' to obtain documentation.",
            file=sys.stderr,
        )
        return 1

    from cxxman.ui.renderer import render_document

    render_document(document, paginate=sys.stdout.isatty())
    return 0


def _search(query: str) -> int:
    try:
        with Database(default_database_path()) as database:
            results = database.search(query, limit=20)
    except (OSError, sqlite3.Error) as error:
        print(f"cxxman: could not search local documentation: {error}", file=sys.stderr)
        return 1
    if not results:
        print("No matching local pages.")
        return 0
    for result in results:
        print(f"{result.canonical_topic}  {result.title}")
    return 0


def _list_pages() -> int:
    try:
        with Database(default_database_path()) as database:
            topics = database.list_topics(limit=100)
    except (OSError, sqlite3.Error) as error:
        print(f"cxxman: could not list local documentation: {error}", file=sys.stderr)
        return 1
    if not topics:
        print("No local pages. Run 'cxxman update' to obtain documentation.")
        return 0
    for topic, title in topics:
        print(f"{topic}  {title}")
    return 0


def _update_pages(topics: list[str]) -> int:
    from cxxman.core.update import UpdateError, update_documents

    try:
        with Database(default_database_path()) as database:
            documents = update_documents(database, topics)
    except (UpdateError, OSError, sqlite3.Error) as error:
        print(f"cxxman: update failed: {error}", file=sys.stderr)
        return 1
    updated_topics = ", ".join(document.canonical_topic for document in documents)
    print(f"Updated {len(documents)} page(s): {updated_topics}")
    return 0
