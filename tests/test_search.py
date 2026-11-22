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


def test_search_ranks_exact_names_before_prefix_purpose_and_body(tmp_path) -> None:
    with Database(tmp_path / "docs.sqlite3") as database:
        database.save_documents(
            [
                Document(
                    "cpp/remove",
                    "std::remove",
                    "remove",
                    "url",
                    (Paragraph("Unrelated body."),),
                ),
                Document(
                    "cpp/remove_duplicates_helper",
                    "std::remove_duplicates_helper",
                    "remove_duplicates_helper",
                    "url",
                    (Paragraph("Remove duplicates in this body."),),
                ),
                Document(
                    "cpp/unique",
                    "std::unique",
                    "unique",
                    "url",
                    (Paragraph("Algorithm body."),),
                    purpose="Removes consecutive duplicate elements from a range.",
                    header="<algorithm>",
                ),
            ]
        )

        results = search_documents(database, "remove duplicates")
        name_ranked = search_documents(database, "remove")

    assert [result.canonical_topic for result in results] == [
        "unique",
        "remove_duplicates_helper",
    ]
    assert results[0].purpose == "Removes consecutive duplicate elements from a range."
    assert results[0].header == "<algorithm>"
    assert [result.canonical_topic for result in name_ranked] == [
        "remove",
        "remove_duplicates_helper",
        "unique",
    ]


def test_exact_alias_lookup_and_search_resolve_to_owning_page(tmp_path) -> None:
    document = Document(
        "cpp/algorithm/remove",
        "std::remove",
        "remove",
        "url",
        (Paragraph("Moves elements."),),
        aliases=("erase_value",),
    )
    with Database(tmp_path / "docs.sqlite3") as database:
        database.save_document(document)

        assert database.get_document("erase_value") == document
        assert search_documents(database, "erase_value")[0].canonical_topic == "remove"
        database._fts_enabled = False
        assert search_documents(database, "erase_value")[0].canonical_topic == "remove"
