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
            documents = update_documents(database, ["vector"], client=client)
            stored = database.get_document("vector")

    assert len(documents) == 1
    assert documents[0] == stored
    assert stored is not None
    assert stored.source_url == "https://en.cppreference.com/cpp/container/vector"
    assert stored.header == "<vector>"
    assert stored.blocks == (
        Heading("std::vector", level=1),
        Paragraph("A sequence container."),
    )
    assert requested_urls == ["https://en.cppreference.com/cpp/container/vector"]


def test_failed_batch_update_preserves_existing_pages(tmp_path) -> None:
    old_document = Document(
        "cppref/container/vector",
        "Old vector page",
        "vector",
        "https://example.test/old",
        (Paragraph("last valid content"),),
    )

    def handler(request: httpx.Request) -> httpx.Response:
        if request.url.path.endswith("/sort"):
            return httpx.Response(503, text="temporary failure")
        return httpx.Response(200, text=PAGE_HTML)

    with httpx.Client(transport=httpx.MockTransport(handler)) as client:
        with Database(tmp_path / "docs.sqlite3") as database:
            database.save_document(old_document)

            with pytest.raises(UpdateError, match="could not download 'sort'"):
                update_documents(database, ["vector", "sort"], client=client)

            assert database.get_document("vector") == old_document
            assert database.get_document("sort") is None


def test_update_rejects_unsupported_topics_before_network(tmp_path) -> None:
    def fail_if_called(request: httpx.Request) -> httpx.Response:
        raise AssertionError("unsupported topics must be rejected before HTTP")

    with httpx.Client(transport=httpx.MockTransport(fail_if_called)) as client:
        with Database(tmp_path / "docs.sqlite3") as database:
            with pytest.raises(UpdateError, match="unsupported topic"):
                update_documents(database, ["not-supported"], client=client)


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
            )
            stored = database.get_document("std::vector::push_back")

    assert len(documents) == 1
    assert documents[0].canonical_topic == "vector::push_back"
    assert stored == documents[0]
    assert requested_urls == [
        "https://en.cppreference.com/cpp/container/vector/push_back"
    ]
