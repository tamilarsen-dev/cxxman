"""Explicit cppreference download and transactional document updates."""

from __future__ import annotations

from dataclasses import dataclass
from datetime import UTC, datetime
from typing import TYPE_CHECKING

import httpx

from cxxman.parser.html_parser import parse_html

if TYPE_CHECKING:
    from cxxman.core.document import Document
    from cxxman.storage.database import Database


@dataclass(frozen=True, slots=True)
class SourcePage:
    """Source metadata for one supported starter reference page."""

    topic: str
    identifier: str
    url: str
    header: str | None


SOURCE_PAGES = {
    "vector": SourcePage(
        "vector", "cppref/container/vector", "https://en.cppreference.com/cpp/container/vector", "<vector>"
    ),
    "string": SourcePage(
        "string", "cppref/string/basic_string", "https://en.cppreference.com/cpp/string/basic_string", "<string>"
    ),
    "sort": SourcePage(
        "sort", "cppref/algorithm/sort", "https://en.cppreference.com/cpp/algorithm/sort", "<algorithm>"
    ),
    "vector::push_back": SourcePage(
        "vector::push_back",
        "cppref/container/vector/push_back",
        "https://en.cppreference.com/cpp/container/vector/push_back",
        "<vector>",
    ),
}


class UpdateError(RuntimeError):
    """An expected fetch, status, or source-page parsing failure."""


def update_documents(
    database: Database,
    topics: tuple[str, ...] | list[str] = (),
    *,
    client: httpx.Client | None = None,
) -> list[Document]:
    """Fetch, parse, then atomically store selected supported pages.

    With no topics, update the small curated starter set. Supplying a client
    makes the network boundary injectable for deterministic offline tests.
    """
    selected_pages = _resolve_pages(topics)
    if client is not None:
        documents = _fetch_pages(selected_pages, client)
    else:
        with httpx.Client(
            timeout=20.0,
            follow_redirects=True,
            headers={"User-Agent": "cxxman/0.1 (C++ reference terminal viewer)"},
        ) as http_client:
            documents = _fetch_pages(selected_pages, http_client)
    database.save_documents(documents)
    return documents


def _resolve_pages(topics: tuple[str, ...] | list[str]) -> list[SourcePage]:
    if not topics:
        return list(SOURCE_PAGES.values())
    selected: list[SourcePage] = []
    seen: set[str] = set()
    for requested in topics:
        topic = requested.strip().casefold()
        if topic.startswith("std::"):
            topic = topic.removeprefix("std::")
        page = SOURCE_PAGES.get(topic)
        if page is None:
            available = ", ".join(SOURCE_PAGES)
            raise UpdateError(f"unsupported topic '{requested}'. Available topics: {available}.")
        if page.topic not in seen:
            selected.append(page)
            seen.add(page.topic)
    return selected


def _fetch_pages(pages: list[SourcePage], client: httpx.Client) -> list[Document]:
    documents: list[Document] = []
    for page in pages:
        try:
            response = client.get(page.url)
            response.raise_for_status()
            document = parse_html(
                response.text,
                source_url=page.url,
                identifier=page.identifier,
                canonical_topic=page.topic,
                header=page.header,
                updated_at=datetime.now(UTC).isoformat(),
            )
        except httpx.HTTPError as error:
            raise UpdateError(f"could not download '{page.topic}': {error}") from error
        except (ValueError, TypeError) as error:
            raise UpdateError(f"could not parse '{page.topic}': {error}") from error
        if not document.blocks or document.title == "Untitled reference page":
            raise UpdateError(f"the downloaded '{page.topic}' page did not contain usable content")
        documents.append(document)
    return documents
