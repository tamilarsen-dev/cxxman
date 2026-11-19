"""Tests for bounded local full-text search."""

import pytest

from cxxman.core.document import Document, Paragraph
from cxxman.core.search import SearchResult, search_documents
from cxxman.storage.database import Database


def add_sample_pages(database: Database) -> None:
    database.save_document(
        Document(
            identifier="cpp/vector",
            title="std::vector",
            canonical_topic="vector",
            source_url="https://example.test/vector",
            blocks=(Paragraph("A dynamically sized sequence container."),),
        )
    )
    database.save_document(
        Document(
            identifier="cpp/string",
            title="std::string",
            canonical_topic="string",
            source_url="https://example.test/string",
            blocks=(Paragraph("A string of characters."),),
        )
    )


def test_search_matches_content_and_returns_only_bounded_metadata(tmp_path) -> None:
    with Database(tmp_path / "docs.sqlite3") as database:
        add_sample_pages(database)

        results = search_documents(database, "sequence container", limit=1)

    assert len(results) == 1
    assert results[0] == SearchResult("cpp/vector", "std::vector", "vector")
    assert not hasattr(results[0], "blocks")


def test_search_finds_title_topic_and_handles_empty_or_missing_queries(tmp_path) -> None:
    with Database(tmp_path / "docs.sqlite3") as database:
        add_sample_pages(database)

        assert search_documents(database, "std::vector") == [
            SearchResult("cpp/vector", "std::vector", "vector")
        ]
        assert search_documents(database, "no matching symbol") == []
        assert search_documents(database, "!!!") == []
        assert search_documents(database, "sequ")[0].canonical_topic == "vector"
        assert len(search_documents(database, "std", limit=1)) == 1


def test_search_rejects_nonpositive_limit(tmp_path) -> None:
    with Database(tmp_path / "docs.sqlite3") as database:
        with pytest.raises(ValueError, match="at least 1"):
            search_documents(database, "vector", limit=0)
