"""Command-line interface for cxxman."""

from __future__ import annotations

import argparse
import sys
from collections.abc import Sequence

from cxxman import __version__


def build_parser() -> argparse.ArgumentParser:
    """Create the argument parser for the initial CLI."""
    parser = argparse.ArgumentParser(
        prog="cxxman",
        description="Read C++ reference documentation in your terminal.",
    )
    parser.add_argument(
        "--version",
        action="version",
        version=f"%(prog)s {__version__}",
    )
    parser.add_argument("topic", nargs="?", help="documentation topic to display")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    """Parse arguments and dispatch the requested CLI action."""
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.topic is None:
        parser.print_help()
        return 0

    print(
        f"cxxman: '{args.topic}' is not available locally. "
        "Run 'cxxman update' to obtain documentation.",
        file=sys.stderr,
    )
    return 1
