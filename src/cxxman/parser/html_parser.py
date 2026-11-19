"""Convert reference HTML into the structured document model."""

from __future__ import annotations

import re
from collections.abc import Iterator

from bs4 import BeautifulSoup, Tag

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

_SKIP_IDS = {"mw-navigation", "siteSub", "footer", "catlinks", "toc"}
_SKIP_CLASSES = {"navbox", "infobox", "sistersitebox", "noprint", "mw-editsection"}


def parse_html(
    html: str,
    *,
    source_url: str,
    identifier: str | None = None,
    canonical_topic: str | None = None,
    header: str | None = None,
    updated_at: str | None = None,
) -> Document:
    """Parse a saved or downloaded HTML page into a document.

    The parser accepts common MediaWiki content containers and falls back
    to the body when they are absent. It performs no network or storage I/O.
    """
    soup = BeautifulSoup(html, "html.parser")
    content = (
        soup.find(id="mw-content-text")
        or soup.find(id="bodyContent")
        or soup.body
        or soup
    )
    for node in list(content.find_all(True)):
        if node.parent is None:
            continue
        classes = set(node.get("class", []))
        is_chrome = (
            node.get("id") in _SKIP_IDS
            or bool(classes & _SKIP_CLASSES)
            or node.name in {"nav", "script", "style", "footer"}
        )
        if is_chrome:
            node.decompose()

    title_tag = content.find("h1") or soup.find("h1") or soup.find("title")
    title = _text(title_tag) if title_tag else "Untitled reference page"
    topic = canonical_topic or title.rsplit("::", 1)[-1].strip()
    blocks = tuple(_parse_children(content))
    return Document(
        identifier=identifier or topic,
        title=title,
        canonical_topic=topic,
        source_url=source_url,
        blocks=blocks,
        header=header,
        updated_at=updated_at,
    )


def _parse_children(parent: Tag) -> Iterator[ContentBlock]:
    for child in parent.children:
        if not isinstance(child, Tag):
            continue
        name = child.name or ""
        if re.fullmatch(r"h[1-6]", name):
            text = _text(child)
            if text:
                yield Heading(text, int(name[1]))
        elif name == "p":
            text = _text(child)
            if text:
                links = child.find_all("a", href=True)
                if len(links) == 1 and _text(links[0]) == text:
                    yield Reference(_text(links[0]), str(links[0]["href"]))
                else:
                    yield Paragraph(text)
        elif name == "pre":
            code = _text(child, preserve=True)
            if code:
                yield CodeBlock(code, language="cpp")
        elif name in {"ul", "ol"}:
            items = tuple(_text(item) for item in child.find_all("li", recursive=False))
            items = tuple(item for item in items if item)
            if items:
                yield ListBlock(items, ordered=name == "ol")
        elif name == "table":
            table = _parse_table(child)
            if table is not None:
                yield table
        elif name in {"div", "section", "main", "article"}:
            yield from _parse_children(child)


def _parse_table(tag: Tag) -> Table | None:
    row_tags = tag.find_all("tr")
    if not row_tags:
        return None
    parsed = [
        tuple(_text(cell) for cell in row.find_all(["th", "td"], recursive=False))
        for row in row_tags
    ]
    parsed = [row for row in parsed if row]
    if not parsed:
        return None
    has_header = bool(row_tags[0].find_all("th", recursive=False))
    headers = parsed.pop(0) if has_header else ()
    width = len(headers) if headers else max(map(len, parsed), default=0)
    rows = tuple(row + ("",) * (width - len(row)) for row in parsed)
    return Table(rows=rows, headers=headers)


def _text(tag: Tag, *, preserve: bool = False) -> str:
    text = tag.get_text("", strip=False)
    if preserve:
        return text.strip("\n\r")
    return " ".join(text.split())
