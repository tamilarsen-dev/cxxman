"""Render structured reference documents in a terminal with Rich."""

from __future__ import annotations

from contextlib import nullcontext
from typing import Literal

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
from cxxman.core.manual import ManualPage, manual_page_line_count

ColorMode = Literal["auto", "always", "never"]


def create_console(color_mode: ColorMode = "auto") -> Console:
    """Create a Rich console with explicit terminal color behavior."""
    if color_mode == "always":
        return Console(highlight=False, force_terminal=True, color_system="truecolor")
    if color_mode == "never":
        return Console(highlight=False, no_color=True, color_system=None)
    return Console(highlight=False)


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
    output = console or create_console()
    use_pager = paginate and output.is_terminal
    pager = output.pager(styles=True) if use_pager else nullcontext()
    with pager:
        output.print(Text(document.title, style="bold cyan"))
        if document.header:
            output.print(Text(document.header, style="dim"))
        output.print()
        for block in document.blocks:
            _render_block(output, block)


def render_manual_page(
    page: ManualPage,
    *,
    console: Console | None = None,
    paginate: bool = False,
) -> None:
    """Render a projected page using concise, optional manual sections."""
    output = console or create_console()
    use_pager = paginate and output.is_terminal and not _manual_page_fits(page, output)
    pager = output.pager(styles=True) if use_pager else nullcontext()
    with pager:
        title = Text(page.name, style="bold cyan")
        if page.purpose:
            title.append(" — ")
            title.append(page.purpose)
        output.print(title)
        if page.header:
            output.print(Text(f"#include {page.header}"))
        output.print()
        for section in page.sections:
            if section.blocks:
                output.print(Text(section.title, style="bold cyan"))
                for block in section.blocks:
                    _render_block(output, block, trailing_blank=False)
            if section.hint:
                output.print(Text(section.hint, style="dim"))
        if page.source_url:
            output.print(
                Text(
                    f"More: cxxman {page.topic or page.name} --full · {page.source_url}",
                    style="dim",
                )
            )


def _render_block(
    console: Console, block: ContentBlock, *, trailing_blank: bool = True
) -> None:
    if isinstance(block, Heading):
        style = "bold cyan" if block.level <= 2 else "bold"
        console.print(Text(block.text, style=style))
    elif isinstance(block, Paragraph):
        console.print(Text(block.text))
    elif isinstance(block, CodeBlock):
        if block.label or block.availability:
            annotation = " ".join(
                part for part in (block.label, block.availability) if part
            )
            console.print(Text(annotation, style="dim"))
        syntax = Syntax(
            block.code,
            block.language or "text",
            theme="monokai",
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
    if trailing_blank:
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


def _manual_page_fits(page: ManualPage, console: Console) -> bool:
    """Avoid opening a pager when this card is likely to fit one screen."""
    width = max(20, console.width)
    height = console.height
    if height <= 0:
        return False
    return manual_page_line_count(page, width) <= height
