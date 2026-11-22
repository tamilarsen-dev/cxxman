"""Offline tests for explicit network ingestion and atomic updates."""

import httpx
import pytest

from cxxman.core.document import Document, Heading, Paragraph
from cxxman.core.update import UpdateError, update_documents
from cxxman.storage.database import Database

PAGE_HTML = "<html><body><h1>std::vector</h1><p>A sequence container.</p></body></html>"
FUNCTION_HTML = (
    "<html><body><h1>std::vector&lt;T,Allocator&gt;::push_back</h1>"
    "<table class='t-dcl-begin'><tr class='t-dcl'>"
    "<td>void push_back(const T&amp; value);</td></tr></table>"
    "<p>Appends value.</p></body></html>"
)


def test_update_fetches_parses_and_stores_selected_page(tmp_path) -> None:
    requested_urls: list[str] = []

    def handler(request: httpx.Request) -> httpx.Response:
        requested_urls.append(str(request.url))
        return httpx.Response(200, text=PAGE_HTML)

    with httpx.Client(transport=httpx.MockTransport(handler)) as client:
        with Database(tmp_path / "docs.sqlite3") as database:
            report = update_documents(database, ["vector"], client=client, request_interval=0)
            stored = database.get_document("vector")

    assert len(report.updated) == 1
    assert not report.failures
    assert report.updated[0] == stored
    assert stored is not None
    assert stored.source_url == "https://en.cppreference.com/cpp/container/vector"
    assert stored.header == "<vector>"
    assert stored.purpose == "A sequence container."
    assert stored.aliases == ()
    assert stored.blocks == (
        Heading("std::vector", level=1),
        Paragraph("A sequence container."),
    )
    assert requested_urls == ["https://en.cppreference.com/cpp/container/vector"]


def test_update_without_topics_selects_the_full_catalog(tmp_path, monkeypatch) -> None:
    from cxxman.core import update
    from cxxman.core.catalog import SourcePage

    requested_urls: list[str] = []
    pages = {
        page.topic: page
        for page in (
            SourcePage("vector", "container/vector", "<vector>", "class"),
            SourcePage("sort", "algorithm/sort", "<algorithm>", "algorithm"),
        )
    }
    monkeypatch.setattr(update, "SOURCE_PAGES", pages)

    def handler(request: httpx.Request) -> httpx.Response:
        requested_urls.append(str(request.url))
        return httpx.Response(
            200,
            text="<html><body><h1>Reference</h1><p>Catalog fixture.</p></body></html>",
        )

    with httpx.Client(transport=httpx.MockTransport(handler)) as client:
        with Database(tmp_path / "docs.sqlite3") as database:
            report = update_documents(database, client=client, request_interval=0)
            cached_topics = database.list_topics(limit=100)

    assert len(report.updated) == 2
    assert not report.failures
    assert len(cached_topics) == 2
    assert set(requested_urls) == {page.url for page in pages.values()}


def test_no_topic_resolution_covers_every_generated_catalog_entry() -> None:
    from cxxman.core.catalog import SOURCE_PAGES
    from cxxman.core.update import _resolve_pages

    assert len(_resolve_pages([])) == len(SOURCE_PAGES) >= 2_000


def test_page_commit_survives_a_later_page_failure(tmp_path) -> None:
    old_sort = Document(
        "cppref/algorithm/sort",
        "Old sort page",
        "sort",
        "https://example.test/old-sort",
        (Paragraph("last valid sort content"),),
    )

    def handler(request: httpx.Request) -> httpx.Response:
        if request.url.path.endswith("/sort"):
            return httpx.Response(503, text="temporary failure")
        return httpx.Response(200, text=PAGE_HTML)

    with httpx.Client(transport=httpx.MockTransport(handler)) as client:
        with Database(tmp_path / "docs.sqlite3") as database:
            database.save_document(old_sort)

            report = update_documents(
                database,
                ["vector", "sort"],
                client=client,
                request_interval=0,
                max_attempts=1,
            )

            assert database.get_document("vector") is not None
            assert database.get_document("sort") == old_sort
            assert tuple(failure.topic for failure in report.failures) == ("sort",)


def test_update_rejects_unsupported_topics_before_network(tmp_path) -> None:
    def fail_if_called(request: httpx.Request) -> httpx.Response:
        raise AssertionError("unsupported topics must be rejected before HTTP")

    with httpx.Client(transport=httpx.MockTransport(fail_if_called)) as client:
        with Database(tmp_path / "docs.sqlite3") as database:
            with pytest.raises(UpdateError, match="unsupported topic"):
                update_documents(database, ["not-supported"], client=client, request_interval=0)


def test_update_accepts_fully_qualified_function_topic(tmp_path) -> None:
    requested_urls: list[str] = []

    def handler(request: httpx.Request) -> httpx.Response:
        requested_urls.append(str(request.url))
        return httpx.Response(200, text=FUNCTION_HTML)

    with httpx.Client(transport=httpx.MockTransport(handler)) as client:
        with Database(tmp_path / "docs.sqlite3") as database:
            documents = update_documents(
                database,
                ["std::vector::push_back"],
                client=client,
                request_interval=0,
            )
            stored = database.get_document("std::vector::push_back")

    assert len(documents.updated) == 1
    assert documents.updated[0].canonical_topic == "vector::push_back"
    assert stored == documents.updated[0]
    assert requested_urls == [
        "https://en.cppreference.com/cpp/container/vector/push_back"
    ]


def test_update_accepts_catalog_alias(tmp_path, monkeypatch: pytest.MonkeyPatch) -> None:
    from dataclasses import replace

    from cxxman.core import catalog

    source_page = catalog.SOURCE_PAGES["vector"]
    monkeypatch.setitem(
        catalog.SOURCE_PAGES,
        "vector",
        replace(source_page, aliases=("dynamic_sequence",)),
    )

    def handler(request: httpx.Request) -> httpx.Response:
        return httpx.Response(200, text=PAGE_HTML)

    with httpx.Client(transport=httpx.MockTransport(handler)) as client:
        with Database(tmp_path / "docs.sqlite3") as database:
            report = update_documents(
                database, ["dynamic_sequence"], client=client, request_interval=0
            )
            stored = database.get_document("dynamic_sequence")

    assert report.updated[0].canonical_topic == "vector"
    assert stored is not None and stored.aliases == ("dynamic_sequence",)


def test_retryable_status_is_retried_then_committed(tmp_path) -> None:
    attempts = 0

    def handler(request: httpx.Request) -> httpx.Response:
        nonlocal attempts
        attempts += 1
        if attempts == 1:
            return httpx.Response(503, request=request)
        return httpx.Response(200, text=PAGE_HTML, request=request)

    with httpx.Client(transport=httpx.MockTransport(handler)) as client:
        with Database(tmp_path / "docs.sqlite3") as database:
            report = update_documents(
                database,
                ["vector"],
                client=client,
                request_interval=0,
                backoff_base=0,
            )

    assert attempts == 2
    assert len(report.updated) == 1
    assert not report.failures


def test_resume_skips_pages_already_stored(tmp_path) -> None:
    requested_urls: list[str] = []

    def handler(request: httpx.Request) -> httpx.Response:
        requested_urls.append(str(request.url))
        return httpx.Response(200, text=PAGE_HTML, request=request)

    with httpx.Client(transport=httpx.MockTransport(handler)) as client:
        with Database(tmp_path / "docs.sqlite3") as database:
            database.save_document(
                Document(
                    "cppref/container/vector",
                    "std::vector",
                    "vector",
                    "https://en.cppreference.com/cpp/container/vector",
                    (Paragraph("cached"),),
                )
            )
            report = update_documents(
                database,
                ["vector", "sort"],
                client=client,
                resume=True,
                request_interval=0,
            )

    assert report.skipped == ("vector",)
    assert [document.canonical_topic for document in report.updated] == ["sort"]
    assert requested_urls == ["https://en.cppreference.com/cpp/algorithm/sort"]


def test_requests_are_spaced_by_the_configured_minimum(tmp_path, monkeypatch) -> None:
    from cxxman.core import update

    current_time = [0.0]
    request_times: list[float] = []
    sleeps: list[float] = []

    def fake_sleep(seconds: float) -> None:
        sleeps.append(seconds)
        current_time[0] += seconds

    monkeypatch.setattr(update.time, "monotonic", lambda: current_time[0])
    monkeypatch.setattr(update.time, "sleep", fake_sleep)

    def handler(request: httpx.Request) -> httpx.Response:
        request_times.append(current_time[0])
        return httpx.Response(200, text=PAGE_HTML, request=request)

    with httpx.Client(transport=httpx.MockTransport(handler)) as client:
        with Database(tmp_path / "docs.sqlite3") as database:
            report = update_documents(
                database,
                ["vector", "sort"],
                client=client,
                request_interval=1.0,
                backoff_base=0,
            )

    assert not report.failures
    assert request_times == [0.0, 1.0]
    assert sleeps == [1.0]
