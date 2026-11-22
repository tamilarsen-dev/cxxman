"""Tests for SQLite document persistence and lookup."""

import json
import sqlite3

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


def test_declaration_metadata_round_trips_and_legacy_content_remains_readable(tmp_path) -> None:
    path = tmp_path / "docs.sqlite3"
    expected = make_document()
    declaration = CodeBlock(
        "void sort();", language="cpp", label="(1)", availability="(since C++20)"
    )
    expected = Document(
        identifier=expected.identifier,
        title=expected.title,
        canonical_topic=expected.canonical_topic,
        source_url=expected.source_url,
        blocks=(declaration,),
    )
    with Database(path) as database:
        database.save_document(expected)
        assert database.get_document("vector") == expected
        row = database._connection.execute(
            "SELECT format_version FROM documents WHERE identifier = ?",
            (expected.identifier,),
        ).fetchone()
        assert row["format_version"] == 3

    legacy_path = tmp_path / "legacy.sqlite3"
    with sqlite3.connect(legacy_path) as connection:
        connection.execute(
            """CREATE TABLE documents (
                id INTEGER PRIMARY KEY, identifier TEXT NOT NULL UNIQUE,
                title TEXT NOT NULL, canonical_topic TEXT NOT NULL,
                source_url TEXT NOT NULL, header TEXT, updated_at TEXT,
                content_json TEXT NOT NULL, search_text TEXT NOT NULL
            )"""
        )
        connection.execute(
            """INSERT INTO documents
                (identifier, title, canonical_topic, source_url, content_json, search_text)
                VALUES (?, ?, ?, ?, ?, ?)""",
            (
                "legacy/vector",
                "std::vector",
                "vector",
                "https://example.test/vector",
                json.dumps([{"type": "code", "code": "void push_back();", "language": "cpp"}]),
                "std::vector vector void push_back",
            ),
        )
    with Database(legacy_path) as database:
        assert database.get_document("vector").blocks == (
            CodeBlock("void push_back();", language="cpp"),
        )
        version = database._connection.execute(
            "SELECT format_version FROM documents WHERE identifier = ?",
            ("legacy/vector",),
        ).fetchone()["format_version"]
        assert version == 1
        assert database.document_needs_update("vector")
        assert database.get_document("vector").purpose is None
        assert database.get_document("vector").aliases == ()


def test_purpose_aliases_round_trip_and_current_format_is_fresh(tmp_path) -> None:
    expected = Document(
        identifier="cpp/vector",
        title="std::vector",
        canonical_topic="vector",
        source_url="https://example.test/vector",
        blocks=(Paragraph("A dynamically sized sequence container."),),
        purpose="A dynamically sized sequence container.",
        aliases=("dynamic_array", "sequence"),
    )
    with Database(tmp_path / "docs.sqlite3") as database:
        database.save_document(expected)

        assert database.get_document("vector") == expected
        assert not database.document_needs_update("vector")


def test_migrates_format_two_rows_without_losing_content(tmp_path) -> None:
    path = tmp_path / "format-two.sqlite3"
    with sqlite3.connect(path) as connection:
        connection.execute(
            """CREATE TABLE documents (
                id INTEGER PRIMARY KEY, identifier TEXT NOT NULL UNIQUE,
                title TEXT NOT NULL, canonical_topic TEXT NOT NULL,
                source_url TEXT NOT NULL, header TEXT, updated_at TEXT,
                content_json TEXT NOT NULL, search_text TEXT NOT NULL,
                format_version INTEGER NOT NULL DEFAULT 1
            )"""
        )
        connection.execute(
            """INSERT INTO documents
                (identifier, title, canonical_topic, source_url, content_json,
                 search_text, format_version)
                VALUES (?, ?, ?, ?, ?, ?, ?)""",
            (
                "cpp/vector",
                "std::vector",
                "vector",
                "https://example.test/vector",
                json.dumps([{"type": "paragraph", "text": "cached content"}]),
                "cached content",
                2,
            ),
        )

    with Database(path) as database:
        stored = database.get_document("vector")
        assert stored.blocks == (Paragraph("cached content"),)
        assert stored.purpose is None
        assert stored.aliases == ()
        assert database.document_needs_update("vector")
        columns = {
            row["name"] for row in database._connection.execute("PRAGMA table_info(documents)")
        }
        assert {"purpose", "aliases_json", "format_version"} <= columns


def test_rebuilds_older_fts_layout_and_indexes_purpose_and_aliases(tmp_path) -> None:
    path = tmp_path / "old-fts.sqlite3"
    with sqlite3.connect(path) as connection:
        connection.execute(
            """CREATE TABLE documents (
                id INTEGER PRIMARY KEY, identifier TEXT NOT NULL UNIQUE,
                title TEXT NOT NULL, canonical_topic TEXT NOT NULL,
                source_url TEXT NOT NULL, header TEXT, updated_at TEXT,
                content_json TEXT NOT NULL, search_text TEXT NOT NULL,
                format_version INTEGER NOT NULL DEFAULT 3,
                purpose TEXT, aliases_json TEXT NOT NULL DEFAULT '[]'
            )"""
        )
        connection.execute(
            """INSERT INTO documents
                (id, identifier, title, canonical_topic, source_url, content_json,
                 search_text, format_version, purpose, aliases_json)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                1,
                "cpp/unique",
                "std::unique",
                "unique",
                "https://example.test/unique",
                json.dumps([{"type": "paragraph", "text": "Algorithm."}]),
                "std::unique unique Algorithm",
                3,
                "Removes consecutive duplicate elements from a range.",
                json.dumps(["remove_duplicates"]),
            ),
        )
        connection.execute(
            """CREATE VIRTUAL TABLE documents_fts USING fts5(
                identifier UNINDEXED, title, canonical_topic, body,
                tokenize='unicode61'
            )"""
        )
        connection.execute(
            """INSERT INTO documents_fts
                (rowid, identifier, title, canonical_topic, body)
                VALUES (?, ?, ?, ?, ?)""",
            (1, "cpp/unique", "std::unique", "unique", "std::unique unique Algorithm"),
        )

    with Database(path) as database:
        assert database.get_document("remove_duplicates").canonical_topic == "unique"
        assert database.search("remove duplicates")[0].purpose.startswith("Removes")


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
