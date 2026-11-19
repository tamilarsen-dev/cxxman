"""Tests for SQLite document persistence and lookup."""

import pytest

from cxxman.core.document import (
    CodeBlock,
    Document,
    Heading,
    ListBlock,
    Paragraph,
    Reference,
    Table,
)
from cxxman.storage.database import Database
from cxxman.storage.database import CorruptDocumentError


def make_document(
    *, identifier: str = "cpp/vector", title: str = "std::vector", topic: str = "vector"
) -> Document:
    return Document(
        identifier=identifier,
        title=title,
        canonical_topic=topic,
        source_url="https://example.test/vector",
        header="<vector>",
        updated_at="2026-11-19T00:00:00Z",
        blocks=(
            Heading("Overview", level=1),
            Paragraph("A dynamically sized sequence container."),
            CodeBlock("void push_back(const T& value);", language="cpp"),
            ListBlock(("push_back", "pop_back")),
            Table((("push_back", "Amortized constant"),), headers=("Operation", "Complexity")),
            Reference("Container requirements", "container"),
        ),
    )


def test_document_round_trip_and_exact_topic_lookup(tmp_path) -> None:
    expected = make_document()
    with Database(tmp_path / "docs.sqlite3") as database:
        database.save_document(expected)

        assert database.get_document("VECTOR") == expected
        assert database.get_document("cpp/vector") == expected
        assert database.get_document("missing") is None


def test_saving_same_identifier_replaces_previous_page(tmp_path) -> None:
    with Database(tmp_path / "docs.sqlite3") as database:
        database.save_document(make_document())
        updated = make_document(title="std::vector (updated)")
        database.save_document(updated)

        assert database.get_document("vector") == updated
        assert database.list_topics() == [("vector", "std::vector (updated)")]


def test_list_topics_is_bounded_and_sorted(tmp_path) -> None:
    with Database(tmp_path / "docs.sqlite3") as database:
        database.save_document(make_document(identifier="cpp/string", title="std::string", topic="string"))
        database.save_document(make_document())

        assert database.list_topics(limit=1) == [("string", "std::string")]


def test_corrupt_structured_record_raises_specific_error(tmp_path) -> None:
    with Database(tmp_path / "docs.sqlite3") as database:
        database.save_document(make_document())
        with database._connection:
            database._connection.execute(
                "UPDATE documents SET content_json = ? WHERE identifier = ?",
                ("not valid JSON", "cpp/vector"),
            )

        with pytest.raises(CorruptDocumentError, match="cpp/vector"):
            database.get_document("vector")
