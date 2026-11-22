"""Convert reference HTML into the structured document model."""

from __future__ import annotations

import re
from copy import deepcopy
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
_SKIP_CLASSES = {
    "navbox",
    "infobox",
    "sistersitebox",
    "noprint",
    "mw-editsection",
    "t-navbar",
    "t-page-template",
    "ambox",
    "mbox-small-left",
    "mw-cite-backlink",
}


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
            or (
                node.name == "a"
                and node.get("href", "").startswith("#cite_ref-")
                and _text(node) == "↑"
            )
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
        elif name == "div" and any(
            re.fullmatch(r"t-li\d*", class_name)
            for class_name in child.get("class", [])
        ):
            text = _text(child)
            if text:
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
            classes = set(child.get("class", []))
            if "t-dcl-begin" in classes:
                yield from _parse_declarations(child)
            else:
                table = _parse_table(child)
                if table is not None:
                    yield table
        elif name in {"div", "section", "main", "article"}:
            yield from _parse_children(child)


def _parse_table(tag: Tag) -> Table | None:
    row_tags = [row for row in tag.find_all("tr") if row.find_parent("table") is tag]
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
    width = max(len(headers), max(map(len, parsed), default=0))
    if headers:
        headers = headers + ("",) * (width - len(headers))
    rows = tuple(row + ("",) * (width - len(row)) for row in parsed)
    return Table(rows=rows, headers=headers)


def _parse_declarations(tag: Tag) -> Iterator[CodeBlock]:
    """Preserve cppreference declaration rows as function signatures."""
    for row in tag.find_all("tr"):
        if row.find_parent("table") is not tag:
            continue
        classes = set(row.get("class", []))
        if not classes & {"t-dcl", "t-dcl-rev"}:
            continue
        cells = row.find_all(["th", "td"], recursive=False)
        if cells:
            signature_node = cells[0].find("pre") or cells[0]
            signature = " ".join(_text(signature_node, preserve=True).split())
            if signature:
                label = _text(cells[1]) if len(cells) > 1 else ""
                availability = _text(cells[2]) if len(cells) > 2 else ""
                yield CodeBlock(
                    signature,
                    language="cpp",
                    label=label or None,
                    availability=availability or None,
                )


def _text(tag: Tag, *, preserve: bool = False) -> str:
    revision_tables = tag.find_all("table", class_="t-rev-begin")
    if revision_tables and not preserve:
        outer = deepcopy(tag)
        for table in outer.find_all("table", class_="t-rev-begin"):
            table.decompose()
        base = " ".join(outer.get_text(" ", strip=True).split())
        revisions: list[str] = []
        for table in revision_tables:
            for row in table.find_all("tr"):
                if row.find_parent("table") is not table:
                    continue
                cells = row.find_all(["th", "td"], recursive=False)
                parts = [_text(cell) for cell in cells]
                value = " ".join(part for part in parts if part)
                if value:
                    revisions.append(value)
        return "\n".join(part for part in (base, *revisions) if part)
    line_groups = tag.select(".t-lines")
    if line_groups and not preserve:
        line_text: list[str] = []
        revisions: list[str] = []
        for group in line_groups:
            for child in group.find_all(recursive=False):
                value = child.get_text("", strip=True)
                if value:
                    revision_marks = child.select(".t-mark-rev")
                    if revision_marks and not child.get_text("", strip=True).replace(
                        " ".join(mark.get_text(" ", strip=True) for mark in revision_marks), ""
                    ).strip():
                        revisions.extend(mark.get_text(" ", strip=True) for mark in revision_marks)
                    else:
                        line_text.append(value)
        text = ", ".join(line_text)
        for mark in tag.select(".t-mark-rev"):
            value = mark.get_text(" ", strip=True)
            if value and value not in revisions:
                revisions.append(value)
        if revisions:
            text = f"{text} {' '.join(revisions)}".strip()
    else:
        text = tag.get_text("", strip=False)
    if preserve:
        return text.strip("\n\r")
    return " ".join(text.split())
