"""Extract standard-library catalog rows from cppreference index HTML."""

from __future__ import annotations

from dataclasses import dataclass
import re
from urllib.parse import urlsplit

from bs4 import BeautifulSoup, Tag


@dataclass(frozen=True, slots=True)
class HeaderIndexEntry:
    """One source row from a cppreference header index page."""

    topic: str
    aliases: tuple[str, ...]
    source_path: str
    entity_kind: str
    purpose: str | None
    group: str | None


_EXCLUDED_SCOPE_COMPONENTS = frozenset({"experimental", "ts"})


def parse_header_listing(html: str) -> tuple[tuple[str, str], ...]:
    """Return ``(header name, source path)`` pairs from the header index."""
    soup = BeautifulSoup(html, "html.parser")
    content = _content(soup)
    headers: list[tuple[str, str]] = []
    seen: set[str] = set()
    for link in content.find_all("a", href=True):
        source_path = _cpp_path(str(link["href"]))
        if (
            source_path is None
            or not source_path.startswith("header/")
            or _has_excluded_scope_component(source_path)
        ):
            continue
        name = source_path.removeprefix("header/").strip("/")
        if not name or "/" in name or name.casefold() in seen:
            continue
        seen.add(name.casefold())
        headers.append((name, source_path))
    return tuple(headers)


def parse_header_index(html: str, *, header: str) -> tuple[HeaderIndexEntry, ...]:
    """Extract names, aliases, groups, purpose text and source paths."""
    soup = BeautifulSoup(html, "html.parser")
    content = _content(soup)
    groups: list[tuple[int, str]] = []
    entries: list[HeaderIndexEntry] = []

    for node in content.find_all(re.compile(r"^(h[2-6]|tr)$")):
        if node.name in {f"h{level}" for level in range(2, 7)}:
            heading_text = _text_without_chrome(node)
            if not heading_text:
                continue
            level = int(node.name[1])
            while groups and groups[-1][0] >= level:
                groups.pop()
            groups.append((level, heading_text))
            continue
        if "t-dsc" not in node.get("class", []):
            continue
        table = node.find_parent("table")
        if table is None or "t-dsc-begin" not in table.get("class", []):
            continue
        if node.find_parent("tr") is not None:
            continue
        cells = node.find_all(["th", "td"], recursive=False)
        if len(cells) < 2:
            continue

        link = cells[0].find("a", href=True)
        if link is None:
            continue
        source_path = _cpp_path(str(link["href"]))
        if source_path is None or not _is_library_path(source_path):
            continue
        names = _names_from_link(link)
        if not names:
            continue
        topic = names[0]
        purpose = _purpose(cells[1])
        kind = _entity_kind(cells[1], source_path)
        group = " / ".join(name for _, name in groups) or None
        entries.append(
            HeaderIndexEntry(
                topic=topic,
                aliases=tuple(name for name in names[1:] if name.casefold() != topic.casefold()),
                source_path=source_path,
                entity_kind=kind,
                purpose=purpose,
                group=group,
            )
        )
    return tuple(entries)


def parse_symbol_index_links(html: str) -> tuple[tuple[str, str], ...]:
    """Return visible C++ reference links from a cppreference symbol index."""
    soup = BeautifulSoup(html, "html.parser")
    content = _content(soup)
    links: list[tuple[str, str]] = []
    seen: set[str] = set()
    for link in content.find_all("a", href=True):
        source_path = _cpp_path(str(link["href"]))
        if source_path is None or not _is_library_path(source_path):
            continue
        label = " ".join(link.get_text(" ", strip=True).split())
        if not label or _is_library_overview(label) or source_path in seen:
            continue
        seen.add(source_path)
        links.append((label, source_path))
    return tuple(links)


def parse_symbol_index_pages(html: str) -> tuple[str, ...]:
    """Return linked cppreference symbol-index subpages."""
    soup = BeautifulSoup(html, "html.parser")
    content = _content(soup)
    pages: list[str] = []
    for link in content.find_all("a", href=True):
        source_path = _cpp_path(str(link["href"]))
        if source_path and source_path.startswith("symbol_index/"):
            if source_path not in pages:
                pages.append(source_path)
    return tuple(pages)


def _content(soup: BeautifulSoup) -> Tag:
    return (
        soup.select_one("#mw-content-text .mw-parser-output")
        or soup.find(id="mw-content-text")
        or soup.find(id="bodyContent")
        or soup.body
        or soup
    )


def _cpp_path(href: str) -> str | None:
    path = urlsplit(href).path
    match = re.match(r"^/(?:w/)?cpp/(.+)$", path)
    if match is None:
        return None
    return match.group(1).split("#", 1)[0].strip("/")


def _is_library_path(source_path: str) -> bool:
    excluded = (
        "language/",
        "preprocessor/",
        "compiler_support/",
        "experimental/",
        "keyword/",
        "header/",
        "symbol_index",
    )
    return (
        bool(source_path)
        and not source_path.startswith(excluded)
        and not _has_excluded_scope_component(source_path)
    )


def _has_excluded_scope_component(source_path: str) -> bool:
    """Return whether a cppreference route belongs to experimental or TS scope."""
    return any(
        component.casefold() in _EXCLUDED_SCOPE_COMPONENTS
        for component in source_path.split("/")
    )


def _is_library_overview(label: str) -> bool:
    """Exclude index/overview articles that do not describe a library entity."""
    normalized = " ".join(label.casefold().split())
    return normalized.endswith(" library")


def _names_from_link(link: Tag) -> list[str]:
    result: list[str] = []
    for group in link.select(".t-lines"):
        for span in group.find_all("span", recursive=False):
            for revision in span.select(".t-mark-rev"):
                revision.decompose()
            name = " ".join(span.get_text("", strip=True).split())
            if name:
                result.append(name.removeprefix("std::"))
    if result:
        return list(dict.fromkeys(result))
    name = " ".join(link.get_text(" ", strip=True).split())
    return [name.removeprefix("std::")] if name else []


def _text_without_chrome(node: Tag) -> str:
    copied = BeautifulSoup(str(node), "html.parser")
    for child in copied.select(".mw-editsection, .editsection, .noprint"):
        child.decompose()
    return " ".join(copied.get_text(" ", strip=True).split())


def _purpose(cell: Tag) -> str | None:
    copied = BeautifulSoup(str(cell), "html.parser")
    for child in copied.select(".t-mark, .t-mark-rev, .mw-editsection, .editsection, .noprint"):
        child.decompose()
    text = " ".join(copied.get_text(" ", strip=True).split())
    if not text:
        return None
    if text.startswith(("std ::", "std::")) and not any(mark in text for mark in ".!?;"):
        return None
    match = re.search(r".*?[.!?](?=\s+[A-Z\"'(`\[]|$)|.+$", text)
    return match.group(0).strip() if match else text


def _entity_kind(cell: Tag, source_path: str) -> str:
    marks = " ".join(
        " ".join(mark.get_text(" ", strip=True).split())
        for mark in cell.select(".t-mark")
    ).casefold()
    if "member function" in marks:
        return "member function"
    if "class template" in marks or "class" in marks:
        return "class"
    if "algorithm" in marks:
        return "algorithm"
    if "function" in marks:
        return "function"
    if "type alias" in marks or "alias" in marks:
        return "alias"
    if "concept" in marks:
        return "concept"
    if "enumeration" in marks or "enum" in marks:
        return "enum"
    if "variable" in marks or "object" in marks:
        return "object"
    if "constant" in marks:
        return "constant"
    if source_path.startswith("concepts/"):
        return "concept"
    return "unknown"
