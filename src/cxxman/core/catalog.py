"""Lazily loaded metadata for supported cppreference pages."""

from __future__ import annotations

from collections.abc import Iterator, MutableMapping
from dataclasses import dataclass
from functools import lru_cache
import json
from pathlib import Path


_CATALOG_PATH = Path(__file__).resolve().parents[1] / "data" / "catalog.json"
_SOURCE_BASE = "https://en.cppreference.com/cpp/"


@dataclass(frozen=True, slots=True)
class SourcePage:
    """Catalog metadata for one C++ standard-library reference page."""

    topic: str
    source_path: str
    header: str | None
    entity_kind: str
    aliases: tuple[str, ...] = ()
    purpose: str | None = None
    parent: str | None = None
    group: str | None = None
    default_update: bool = False

    @property
    def identifier(self) -> str:
        """Return the stable local identifier derived from its source path."""
        return f"cppref/{self.source_path}"

    @property
    def url(self) -> str:
        """Return the cppreference URL for this source path."""
        return _SOURCE_BASE + self.source_path


@lru_cache(maxsize=1)
def _load_pages() -> dict[str, SourcePage]:
    try:
        data = json.loads(_CATALOG_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeError(f"could not load the generated page catalog: {error}") from error
    if data.get("format_version") != 1 or not isinstance(data.get("entries"), list):
        raise RuntimeError("the generated page catalog has an unsupported format")

    pages: dict[str, SourcePage] = {}
    for item in data["entries"]:
        page = SourcePage(
            topic=str(item["topic"]),
            source_path=str(item["source_path"]),
            header=str(item["header"]) if item.get("header") else None,
            entity_kind=str(item.get("entity_kind") or "unknown"),
            aliases=tuple(str(alias) for alias in item.get("aliases", ())),
            purpose=str(item["purpose"]) if item.get("purpose") else None,
            parent=str(item["parent"]) if item.get("parent") else None,
            group=str(item["group"]) if item.get("group") else None,
            default_update=bool(item.get("default_update", False)),
        )
        key = page.topic.casefold()
        if key in pages:
            raise RuntimeError(f"duplicate canonical topic in generated catalog: {page.topic}")
        pages[key] = page
    return pages


class _LazyPages(MutableMapping[str, SourcePage]):
    """Mapping facade that reads catalog metadata on first access."""

    def __getitem__(self, key: str) -> SourcePage:
        return _load_pages()[key.casefold()]

    def __setitem__(self, key: str, value: SourcePage) -> None:
        _load_pages()[key.casefold()] = value

    def __delitem__(self, key: str) -> None:
        del _load_pages()[key.casefold()]

    def __iter__(self) -> Iterator[str]:
        return iter(_load_pages())

    def __len__(self) -> int:
        return len(_load_pages())


SOURCE_PAGES: MutableMapping[str, SourcePage] = _LazyPages()


def default_update_pages() -> tuple[SourcePage, ...]:
    """Return the pages in the original curated default-update set."""
    return tuple(page for page in SOURCE_PAGES.values() if page.default_update)


def find_source_page(topic: str) -> SourcePage | None:
    """Resolve a canonical topic or one of its documented aliases."""
    normalized = topic.strip().casefold()
    if normalized.startswith("std::"):
        normalized = normalized[5:]
    page = SOURCE_PAGES.get(normalized)
    if page is not None:
        return page
    for candidate in SOURCE_PAGES.values():
        if normalized in {alias.casefold() for alias in candidate.aliases}:
            return candidate
    return None


def pages_for_header(header: str) -> tuple[SourcePage, ...]:
    """Return catalog entities listed under one standard header."""
    normalized = header.strip().casefold().strip("<>")
    return tuple(
        page
        for page in SOURCE_PAGES.values()
        if page.header and page.header.casefold().strip("<>") == normalized
    )


def catalog_headers() -> tuple[str, ...]:
    """Return distinct headers represented in the generated catalog."""
    return tuple(
        dict.fromkeys(
            page.header
            for page in SOURCE_PAGES.values()
            if page.header is not None
        )
    )
