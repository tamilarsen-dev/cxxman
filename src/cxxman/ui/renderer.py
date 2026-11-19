"""Render structured reference documents in a terminal with Rich."""

from __future__ import annotations

from contextlib import nullcontext

from rich import box
from rich.console import Console
from rich.padding import Padding
from rich.syntax import Syntax
from rich.table import Table as RichTable
from rich.text import Text

from cxxman.core.document import (
    CodeBlock,
    ContentBlock,
    Document,
    Heading,
    ListBlock,
    Paragraph,
    Reference,
    Table,
)


def render_document(
    document: Document,
    *,
    console: Console | None = None,
    paginate: bool = False,
) -> None:
    """Print a document, optionally opening a pager for terminal output.

    Rich detects terminal width and disables color when output is piped.
    Pagination is only activated when the destination is a terminal.
    """
    output = console or Console(highlight=False)
    use_pager = paginate and output.is_terminal
    pager = output.pager(styles=True) if use_pager else nullcontext()
    with pager:
        output.print(Text(document.title, style="bold cyan"))
        if document.header:
            output.print(Text(document.header, style="dim"))
        output.print()
        for block in document.blocks:
            _render_block(output, block)


def _render_block(console: Console, block: ContentBlock) -> None:
    if isinstance(block, Heading):
        style = "bold cyan" if block.level <= 2 else "bold"
        console.print(Text(block.text, style=style))
    elif isinstance(block, Paragraph):
        console.print(Text(block.text))
    elif isinstance(block, CodeBlock):
        syntax = Syntax(
            block.code,
            block.language or "text",
            theme="ansi_dark",
            word_wrap=True,
            line_numbers=False,
        )
        console.print(Padding(syntax, (0, 1)))
    elif isinstance(block, ListBlock):
        for index, item in enumerate(block.items, start=1):
            prefix = f"{index}. " if block.ordered else "• "
            console.print(Text(prefix + item), soft_wrap=True)
    elif isinstance(block, Table):
        _render_table(console, block)
    elif isinstance(block, Reference):
        console.print(Text(f"{block.label} ({block.target})", style="underline blue"))
    console.print()


def _render_table(console: Console, table: Table) -> None:
    column_count = len(table.headers) if table.headers else (len(table.rows[0]) if table.rows else 0)
    if column_count == 0:
        return
    rendered = RichTable(box=box.SIMPLE, show_header=bool(table.headers), expand=True)
    headers = table.headers or tuple("" for _ in range(column_count))
    for heading in headers:
        rendered.add_column(heading, overflow="fold", header_style="bold")
    for row in table.rows:
        rendered.add_row(*row)
    console.print(rendered)
