"""Explicit cppreference download and transactional document updates."""

from __future__ import annotations

from datetime import UTC, datetime
from typing import TYPE_CHECKING, Callable

import httpx

from cxxman.core.catalog import SOURCE_PAGES, SourcePage
from cxxman.parser.html_parser import parse_html

if TYPE_CHECKING:
    from cxxman.core.document import Document
    from cxxman.storage.database import Database


class UpdateError(RuntimeError):
    """An expected fetch, status, or source-page parsing failure."""


def update_documents(
    database: Database,
    topics: tuple[str, ...] | list[str] = (),
    *,
    client: httpx.Client | None = None,
    on_page_start: Callable[[SourcePage], None] | None = None,
) -> list[Document]:
    """Fetch, parse, then atomically store selected supported pages.

    With no topics, update the complete curated catalog. Supplying a client
    makes the network boundary injectable for deterministic offline tests.
    ``on_page_start`` may report progress without coupling this service to a
    particular terminal UI.
    """
    selected_pages = _resolve_pages(topics)
    if client is not None:
        documents = _fetch_pages(selected_pages, client, on_page_start)
    else:
        with httpx.Client(
            timeout=20.0,
            follow_redirects=True,
            headers={"User-Agent": "cxxman/0.1 (C++ reference terminal viewer)"},
        ) as http_client:
            documents = _fetch_pages(selected_pages, http_client, on_page_start)
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


def _fetch_pages(
    pages: list[SourcePage],
    client: httpx.Client,
    on_page_start: Callable[[SourcePage], None] | None,
) -> list[Document]:
    documents: list[Document] = []
    for page in pages:
        if on_page_start is not None:
            on_page_start(page)
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
