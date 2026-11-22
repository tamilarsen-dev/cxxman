"""Explicit, rate-limited cppreference downloads with per-page commits."""

from __future__ import annotations

from dataclasses import dataclass, replace
from datetime import UTC, datetime
import sqlite3
import time
from typing import TYPE_CHECKING, Callable

import httpx

from cxxman.core.catalog import SOURCE_PAGES, SourcePage
from cxxman.core.document import Document
from cxxman.parser.html_parser import parse_html

if TYPE_CHECKING:
    from cxxman.storage.database import Database


PROJECT_URL = "https://github.com/tamilarsen-dev/cxxman"
USER_AGENT = f"cxxman catalog updater ({PROJECT_URL})"


class UpdateError(RuntimeError):
    """An expected request to update invalid or unsupported topics."""


@dataclass(frozen=True, slots=True)
class UpdateFailure:
    """One catalog page that failed without stopping other page updates."""

    topic: str
    reason: str


@dataclass(frozen=True, slots=True)
class UpdateReport:
    """Results from a catalog update; each item in ``updated`` is committed."""

    updated: tuple[Document, ...]
    failures: tuple[UpdateFailure, ...] = ()
    skipped: tuple[str, ...] = ()


def update_documents(
    database: Database,
    topics: tuple[str, ...] | list[str] = (),
    *,
    client: httpx.Client | None = None,
    on_page_start: Callable[[SourcePage], None] | None = None,
    resume: bool = False,
    request_interval: float = 1.0,
    max_attempts: int = 3,
    backoff_base: float = 1.0,
) -> UpdateReport:
    """Update selected pages, committing successes and reporting failures.

    With no topics, the complete generated catalog is selected. Each usable
    page is saved before the next one starts, so interruption or a later page
    failure leaves earlier successes intact. ``resume`` skips pages already
    present in the local database. Delays and attempts are injectable for
    deterministic tests; production defaults use a one-second request gap
    and exponential retry backoff for transient HTTP failures.
    """
    if request_interval < 0 or max_attempts < 1 or backoff_base < 0:
        raise ValueError("request interval and backoff must be non-negative; attempts must be positive")
    selected_pages = _resolve_pages(topics)
    if client is not None:
        return _update_pages(
            database,
            selected_pages,
            client,
            on_page_start,
            resume=resume,
            request_interval=request_interval,
            max_attempts=max_attempts,
            backoff_base=backoff_base,
        )

    with httpx.Client(
        timeout=20.0,
        follow_redirects=True,
        headers={"User-Agent": USER_AGENT},
    ) as http_client:
        return _update_pages(
            database,
            selected_pages,
            http_client,
            on_page_start,
            resume=resume,
            request_interval=request_interval,
            max_attempts=max_attempts,
            backoff_base=backoff_base,
        )


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
            page = next(
                (
                    candidate
                    for candidate in SOURCE_PAGES.values()
                    if topic in {alias.casefold() for alias in candidate.aliases}
                ),
                None,
            )
        if page is None:
            raise UpdateError(
                f"unsupported topic '{requested}'. Run 'cxxman topics' to list catalog pages."
            )
        if page.topic.casefold() not in seen:
            selected.append(page)
            seen.add(page.topic.casefold())
    return selected


def _update_pages(
    database: Database,
    pages: list[SourcePage],
    client: httpx.Client,
    on_page_start: Callable[[SourcePage], None] | None,
    *,
    resume: bool,
    request_interval: float,
    max_attempts: int,
    backoff_base: float,
) -> UpdateReport:
    updated: list[Document] = []
    failures: list[UpdateFailure] = []
    skipped: list[str] = []
    last_request: list[float | None] = [None]

    for page in pages:
        if resume:
            try:
                if database.get_document(page.topic) is not None:
                    skipped.append(page.topic)
                    continue
            except ValueError:
                # A corrupt cached copy is not complete work; try to replace it.
                pass
        if on_page_start is not None:
            on_page_start(page)
        try:
            document = _fetch_page(
                page,
                client,
                last_request=last_request,
                request_interval=request_interval,
                max_attempts=max_attempts,
                backoff_base=backoff_base,
            )
            database.save_document(document)
        except (httpx.HTTPError, UpdateError, sqlite3.Error, ValueError, TypeError) as error:
            failures.append(UpdateFailure(page.topic, str(error)))
            continue
        updated.append(document)

    return UpdateReport(tuple(updated), tuple(failures), tuple(skipped))


def _fetch_page(
    page: SourcePage,
    client: httpx.Client,
    *,
    last_request: list[float | None],
    request_interval: float,
    max_attempts: int,
    backoff_base: float,
) -> Document:
    response: httpx.Response | None = None
    for attempt in range(max_attempts):
        if last_request[0] is not None:
            wait = request_interval - (time.monotonic() - last_request[0])
            if wait > 0:
                time.sleep(wait)
        last_request[0] = time.monotonic()
        try:
            response = client.get(page.url)
            response.raise_for_status()
            break
        except httpx.HTTPStatusError as error:
            if not _is_retryable_status(error.response.status_code) or attempt + 1 >= max_attempts:
                raise UpdateError(f"could not download '{page.topic}': {error}") from error
            time.sleep(backoff_base * (2**attempt))
        except httpx.RequestError as error:
            if attempt + 1 >= max_attempts:
                raise UpdateError(f"could not download '{page.topic}': {error}") from error
            time.sleep(backoff_base * (2**attempt))

    if response is None:
        raise UpdateError(f"could not download '{page.topic}': request attempts exhausted")

    try:
        document = parse_html(
            response.text,
            source_url=page.url,
            identifier=page.identifier,
            canonical_topic=page.topic,
            header=page.header,
            updated_at=datetime.now(UTC).isoformat(),
        )
        from cxxman.core.manual import project_document

        document = replace(
            document,
            purpose=project_document(document).purpose,
            aliases=page.aliases,
        )
    except (ValueError, TypeError) as error:
        raise UpdateError(f"could not parse '{page.topic}': {error}") from error
    if not document.blocks or document.title == "Untitled reference page":
        raise UpdateError(f"the downloaded '{page.topic}' page did not contain usable content")
    return document


def _is_retryable_status(status_code: int) -> bool:
    return status_code == 429 or status_code >= 500
