"""SQLite persistence and indexed lookup for structured documents."""

from __future__ import annotations

import json
import os
import re
import sqlite3
from pathlib import Path
from typing import TYPE_CHECKING

from cxxman.core.document import (
    CodeBlock,
    ContentBlock,
    Document,
    Heading,
    ListBlock,
    Paragraph,
    Reference,
    Table,
)

if TYPE_CHECKING:
    from cxxman.core.search import SearchResult

_MAX_RESULTS = 100
_DOCUMENT_FORMAT_VERSION = 3
_TOKEN_PATTERN = re.compile(r"[^\W_]+(?:_[^\W_]+)*", re.UNICODE)


def default_database_path() -> Path:
    """Return the per-user database location for the current platform."""
    configured_path = os.environ.get("CXXMAN_DB_PATH")
    if configured_path:
        return Path(configured_path).expanduser()
    if os.name == "nt":
        data_root = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
    else:
        data_root = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share"))
    return data_root / "cxxman" / "cxxman.sqlite3"


class CorruptDocumentError(ValueError):
    """A stored page record could not be reconstructed as a document."""


class Database:
    """Persist documents in SQLite and provide bounded local queries."""

    def __init__(self, path: str | Path) -> None:
        self.path = str(path)
        if self.path != ":memory:":
            Path(self.path).parent.mkdir(parents=True, exist_ok=True)
        self._connection = sqlite3.connect(self.path)
        self._connection.row_factory = sqlite3.Row
        self._fts_enabled = False
        self._initialize()

    @property
    def fts_enabled(self) -> bool:
        """Whether this SQLite build supports the FTS5 search index."""
        return self._fts_enabled

    def close(self) -> None:
        """Close the underlying SQLite connection."""
        self._connection.close()

    def __enter__(self) -> Database:
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def _initialize(self) -> None:
        connection = self._connection
        connection.execute("BEGIN")
        try:
            self._initialize_schema(connection)
        except BaseException:
            connection.rollback()
            raise
        else:
            connection.commit()

    def _initialize_schema(self, connection: sqlite3.Connection) -> None:
        connection.execute(
            """CREATE TABLE IF NOT EXISTS documents (
                id INTEGER PRIMARY KEY,
                identifier TEXT NOT NULL UNIQUE,
                title TEXT NOT NULL,
                canonical_topic TEXT NOT NULL,
                source_url TEXT NOT NULL,
                header TEXT,
                updated_at TEXT,
                content_json TEXT NOT NULL,
                search_text TEXT NOT NULL,
                format_version INTEGER NOT NULL DEFAULT 1,
                purpose TEXT,
                aliases_json TEXT NOT NULL DEFAULT '[]'
            )"""
        )
        columns = {
            row["name"] for row in connection.execute("PRAGMA table_info(documents)")
        }
        if "format_version" not in columns:
            connection.execute(
                "ALTER TABLE documents ADD COLUMN format_version INTEGER NOT NULL DEFAULT 1"
            )
        if "purpose" not in columns:
            connection.execute("ALTER TABLE documents ADD COLUMN purpose TEXT")
        if "aliases_json" not in columns:
            connection.execute(
                "ALTER TABLE documents ADD COLUMN aliases_json TEXT NOT NULL DEFAULT '[]'"
            )
        connection.execute(
            "CREATE INDEX IF NOT EXISTS documents_topic_idx "
            "ON documents(canonical_topic COLLATE NOCASE)"
        )
        connection.execute(
            "CREATE INDEX IF NOT EXISTS documents_identifier_idx ON documents(identifier)"
        )
        alias_index_exists = connection.execute(
            "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'document_aliases'"
        ).fetchone() is not None
        connection.execute(
            """CREATE TABLE IF NOT EXISTS document_aliases (
                alias TEXT NOT NULL COLLATE NOCASE,
                document_id INTEGER NOT NULL REFERENCES documents(id),
                PRIMARY KEY (alias, document_id)
            )"""
        )
        connection.execute(
            "CREATE INDEX IF NOT EXISTS document_aliases_name_idx "
            "ON document_aliases(alias COLLATE NOCASE)"
        )
        if not alias_index_exists:
            self._backfill_alias_index(connection)

        fts_table_exists = connection.execute(
            "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'documents_fts'"
        ).fetchone() is not None
        fts_columns = (
            {row["name"] for row in connection.execute("PRAGMA table_info(documents_fts)")}
            if fts_table_exists
            else set()
        )
        if fts_table_exists and not {"purpose", "aliases"}.issubset(fts_columns):
            connection.execute("DROP TABLE documents_fts")
            fts_table_exists = False
        try:
            connection.execute(
                """CREATE VIRTUAL TABLE IF NOT EXISTS documents_fts USING fts5(
                    identifier UNINDEXED, title, canonical_topic,
                    purpose, aliases, body,
                    tokenize='unicode61'
                )"""
            )
        except sqlite3.OperationalError as error:
            if "fts5" not in str(error).lower():
                raise
        else:
            self._fts_enabled = True
            if not fts_table_exists:
                connection.execute(
                    """INSERT INTO documents_fts
                            (rowid, identifier, title, canonical_topic, purpose, aliases, body)
                            SELECT id, identifier, title, canonical_topic,
                                   COALESCE(purpose, ''), aliases_json, search_text
                        FROM documents"""
                )

    @staticmethod
    def _backfill_alias_index(connection: sqlite3.Connection) -> None:
        """Build the indexed alias lookup once when upgrading an older database."""
        for row in connection.execute("SELECT id, aliases_json FROM documents"):
            try:
                aliases = json.loads(row["aliases_json"])
            except (json.JSONDecodeError, TypeError):
                continue
            if not isinstance(aliases, list):
                continue
            normalized = {
                alias.strip().casefold()
                for alias in aliases
                if isinstance(alias, str) and alias.strip()
            }
            connection.executemany(
                "INSERT OR IGNORE INTO document_aliases(alias, document_id) VALUES (?, ?)",
                ((alias, row["id"]) for alias in normalized),
            )

    def save_document(self, document: Document) -> None:
        """Insert or replace one page and its search-index entry atomically."""
        self.save_documents((document,))

    def save_documents(self, documents: tuple[Document, ...] | list[Document]) -> None:
        """Persist a batch atomically so a failed update cannot partially commit."""
        prepared = [
            (
                document,
                json.dumps(
                    [_serialize_block(block) for block in document.blocks],
                    ensure_ascii=False,
                    separators=(",", ":"),
                ),
                json.dumps(document.aliases, ensure_ascii=False, separators=(",", ":")),
                " ".join(
                    (
                        document.title,
                        document.canonical_topic,
                        document.purpose or "",
                        " ".join(document.aliases),
                        _document_text(document.blocks),
                    )
                ),
            )
            for document in documents
        ]
        with self._connection:
            for document, content_json, aliases_json, search_text in prepared:
                self._connection.execute(
                    """INSERT INTO documents
                        (identifier, title, canonical_topic, source_url, header,
                         updated_at, content_json, search_text, format_version,
                         purpose, aliases_json)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    ON CONFLICT(identifier) DO UPDATE SET
                        title=excluded.title,
                        canonical_topic=excluded.canonical_topic,
                        source_url=excluded.source_url,
                        header=excluded.header,
                        updated_at=excluded.updated_at,
                        content_json=excluded.content_json,
                        search_text=excluded.search_text,
                        format_version=excluded.format_version,
                        purpose=excluded.purpose,
                        aliases_json=excluded.aliases_json""",
                    (
                        document.identifier,
                        document.title,
                        document.canonical_topic,
                        document.source_url,
                        document.header,
                        document.updated_at,
                        content_json,
                        search_text,
                        _DOCUMENT_FORMAT_VERSION,
                        document.purpose,
                        aliases_json,
                    ),
                )
                row = self._connection.execute(
                    "SELECT id FROM documents WHERE identifier = ?",
                    (document.identifier,),
                ).fetchone()
                row_id = row["id"]
                self._connection.execute(
                    "DELETE FROM document_aliases WHERE document_id = ?", (row_id,)
                )
                normalized_aliases = {
                    alias.strip().casefold()
                    for alias in document.aliases
                    if alias.strip()
                }
                self._connection.executemany(
                    "INSERT INTO document_aliases(alias, document_id) VALUES (?, ?)",
                    ((alias, row_id) for alias in normalized_aliases),
                )
                if self._fts_enabled:
                    self._connection.execute(
                        "DELETE FROM documents_fts WHERE rowid = ?", (row_id,)
                    )
                    self._connection.execute(
                        """INSERT INTO documents_fts
                            (rowid, identifier, title, canonical_topic, purpose, aliases, body)
                        VALUES (?, ?, ?, ?, ?, ?, ?)""",
                        (
                            row_id,
                            document.identifier,
                            document.title,
                            document.canonical_topic,
                            document.purpose or "",
                            " ".join(document.aliases),
                            search_text,
                        ),
                    )

    def get_document(self, topic: str) -> Document | None:
        """Load one page by exact canonical topic or stable identifier."""
        lookup_topic = topic.strip()
        if lookup_topic.casefold().startswith("std::"):
            lookup_topic = lookup_topic[5:]
        row = self._connection.execute(
            """SELECT identifier, title, canonical_topic, source_url, header,
                      updated_at, content_json, purpose, aliases_json
               FROM documents
               WHERE canonical_topic = ? COLLATE NOCASE OR identifier = ? COLLATE NOCASE
                  OR EXISTS (
                      SELECT 1 FROM document_aliases a
                      WHERE a.document_id = documents.id AND a.alias = ? COLLATE NOCASE
                  )
               ORDER BY CASE WHEN canonical_topic = ? COLLATE NOCASE THEN 0 ELSE 1 END,
                        identifier COLLATE NOCASE
               LIMIT 1""",
            (lookup_topic, lookup_topic, lookup_topic.casefold(), lookup_topic),
        ).fetchone()
        if row is None:
            return None
        try:
            return _deserialize_document(row)
        except (json.JSONDecodeError, KeyError, TypeError, ValueError, IndexError) as error:
            raise CorruptDocumentError(
                f"stored page '{row['identifier']}' has invalid structured content"
            ) from error

    def document_needs_update(self, topic: str) -> bool:
        """Return whether a stored page uses an older structured format."""
        lookup_topic = topic.strip()
        if lookup_topic.casefold().startswith("std::"):
            lookup_topic = lookup_topic[5:]
        row = self._connection.execute(
            """SELECT format_version FROM documents
               WHERE canonical_topic = ? COLLATE NOCASE OR identifier = ? COLLATE NOCASE
                  OR EXISTS (
                      SELECT 1 FROM document_aliases a
                      WHERE a.document_id = documents.id AND a.alias = ? COLLATE NOCASE
                  )
               ORDER BY CASE WHEN canonical_topic = ? COLLATE NOCASE THEN 0 ELSE 1 END,
                        identifier COLLATE NOCASE
               LIMIT 1""",
            (lookup_topic, lookup_topic, lookup_topic.casefold(), lookup_topic),
        ).fetchone()
        return row is not None and row["format_version"] < _DOCUMENT_FORMAT_VERSION

    def list_topics(self, limit: int = 100) -> list[tuple[str, str]]:
        """Return bounded ``(canonical_topic, title)`` pairs in stable order."""
        bounded_limit = _validate_limit(limit)
        rows = self._connection.execute(
            """SELECT canonical_topic, title FROM documents
               ORDER BY canonical_topic COLLATE NOCASE, identifier LIMIT ?""",
            (bounded_limit,),
        ).fetchall()
        return [(row["canonical_topic"], row["title"]) for row in rows]

    def search(self, query: str, limit: int = 20) -> list[SearchResult]:
        """Return bounded metadata search results without loading page bodies."""
        from cxxman.core.search import SearchResult

        bounded_limit = _validate_limit(limit)
        terms = _TOKEN_PATTERN.findall(query)
        if not terms:
            return []
        term_variants = [_search_term_variants(term) for term in terms]
        lookup_name = query.strip()
        if lookup_name.casefold().startswith("std::"):
            lookup_name = lookup_name[5:]
        prefix_pattern = _escape_like(lookup_name.casefold()) + "%"
        purpose_predicate = " AND ".join(
            "(" + " OR ".join("d.purpose LIKE ? ESCAPE '\\'" for _ in variants) + ")"
            for variants in term_variants
        )
        purpose_values = [
            f"%{_escape_like(variant)}%"
            for variants in term_variants
            for variant in variants
        ]
        if self._fts_enabled:
            match = " AND ".join(
                "(" + " OR ".join(f'"{variant}"*' for variant in variants) + ")"
                for variants in term_variants
            )
            rows = self._connection.execute(
                f"""SELECT d.identifier, d.title, d.canonical_topic,
                          d.purpose, d.header
                   FROM documents_fts f
                   JOIN documents d ON d.id = f.rowid
                   WHERE documents_fts MATCH ?
                   ORDER BY CASE
                       WHEN d.canonical_topic = ? COLLATE NOCASE OR EXISTS (
                           SELECT 1 FROM document_aliases a
                           WHERE a.document_id = d.id AND a.alias = ? COLLATE NOCASE
                       ) THEN 0
                       WHEN d.canonical_topic LIKE ? ESCAPE '\\' OR EXISTS (
                           SELECT 1 FROM document_aliases a
                           WHERE a.document_id = d.id AND a.alias LIKE ? ESCAPE '\\'
                       ) THEN 1
                       WHEN {purpose_predicate} THEN 2
                       ELSE 3 END,
                            bm25(documents_fts), d.canonical_topic COLLATE NOCASE,
                            d.identifier
                   LIMIT ?""",
                (
                    match,
                    lookup_name,
                    lookup_name.casefold(),
                    prefix_pattern,
                    prefix_pattern,
                    *purpose_values,
                    bounded_limit,
                ),
            ).fetchall()
        else:
            predicates = " AND ".join(
                "(" + " OR ".join(
                    "(search_text LIKE ? ESCAPE '\\' OR "
                    "COALESCE(purpose, '') LIKE ? ESCAPE '\\' OR EXISTS ("
                    "SELECT 1 FROM document_aliases a WHERE a.document_id = documents.id "
                    "AND a.alias LIKE ? ESCAPE '\\'))"
                    for _variant in variants
                ) + ")"
                for variants in term_variants
            )
            purpose_predicate = " AND ".join(
                "(" + " OR ".join("purpose LIKE ? ESCAPE '\\'" for _ in variants) + ")"
                for variants in term_variants
            )
            values = [
                value
                for variants in term_variants
                for variant in variants
                for value in [f"%{_escape_like(variant)}%"] * 3
            ]
            rows = self._connection.execute(
                f"""SELECT identifier, title, canonical_topic, purpose, header FROM documents
                    WHERE {predicates}
                    ORDER BY CASE
                        WHEN canonical_topic = ? COLLATE NOCASE OR EXISTS (
                            SELECT 1 FROM document_aliases a
                            WHERE a.document_id = documents.id AND a.alias = ? COLLATE NOCASE
                        ) THEN 0
                        WHEN canonical_topic LIKE ? ESCAPE '\\' OR EXISTS (
                            SELECT 1 FROM document_aliases a
                            WHERE a.document_id = documents.id AND a.alias LIKE ? ESCAPE '\\'
                        ) THEN 1
                        WHEN {purpose_predicate} THEN 2
                        ELSE 3 END,
                        canonical_topic COLLATE NOCASE, identifier LIMIT ?""",
                (
                    *values,
                    lookup_name,
                    lookup_name.casefold(),
                    prefix_pattern,
                    prefix_pattern,
                    *purpose_values,
                    bounded_limit,
                ),
            ).fetchall()
        return [
            SearchResult(
                row["identifier"],
                row["title"],
                row["canonical_topic"],
                row["purpose"],
                row["header"],
            )
            for row in rows
        ]


def _validate_limit(limit: int) -> int:
    if limit < 1:
        raise ValueError("result limit must be at least 1")
    return min(limit, _MAX_RESULTS)


def _escape_like(value: str) -> str:
    return value.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_")


def _search_term_variants(term: str) -> tuple[str, ...]:
    """Match a simple singular/plural spelling variation without stemming."""
    if len(term) <= 3:
        return (term,)
    alternate = term[:-1] if term.endswith("s") else term + "s"
    return (term, alternate)


def _serialize_block(block: ContentBlock) -> dict[str, object]:
    if isinstance(block, Heading):
        return {"type": "heading", "text": block.text, "level": block.level}
    if isinstance(block, Paragraph):
        return {"type": "paragraph", "text": block.text}
    if isinstance(block, CodeBlock):
        return {
            "type": "code",
            "code": block.code,
            "language": block.language,
            "label": block.label,
            "availability": block.availability,
        }
    if isinstance(block, ListBlock):
        return {"type": "list", "items": block.items, "ordered": block.ordered}
    if isinstance(block, Table):
        return {"type": "table", "headers": block.headers, "rows": block.rows}
    if isinstance(block, Reference):
        return {"type": "reference", "label": block.label, "target": block.target}
    raise TypeError(f"unsupported content block: {type(block).__name__}")


def _deserialize_document(row: sqlite3.Row) -> Document:
    blocks = tuple(_deserialize_block(item) for item in json.loads(row["content_json"]))
    aliases = json.loads(row["aliases_json"])
    if not isinstance(aliases, list) or any(not isinstance(alias, str) for alias in aliases):
        raise ValueError("stored aliases must be a JSON list of strings")
    return Document(
        identifier=row["identifier"],
        title=row["title"],
        canonical_topic=row["canonical_topic"],
        source_url=row["source_url"],
        blocks=blocks,
        header=row["header"],
        updated_at=row["updated_at"],
        purpose=row["purpose"],
        aliases=tuple(aliases),
    )


def _deserialize_block(data: dict[str, object]) -> ContentBlock:
    block_type = data.get("type")
    if block_type == "heading":
        return Heading(str(data["text"]), int(data["level"]))
    if block_type == "paragraph":
        return Paragraph(str(data["text"]))
    if block_type == "code":
        language = data.get("language")
        label = data.get("label")
        availability = data.get("availability")
        return CodeBlock(
            str(data["code"]),
            str(language) if language is not None else None,
            str(label) if label is not None else None,
            str(availability) if availability is not None else None,
        )
    if block_type == "list":
        return ListBlock(tuple(str(item) for item in data["items"]), bool(data["ordered"]))
    if block_type == "table":
        return Table(
            rows=tuple(tuple(str(cell) for cell in row) for row in data["rows"]),
            headers=tuple(str(cell) for cell in data["headers"]),
        )
    if block_type == "reference":
        return Reference(str(data["label"]), str(data["target"]))
    raise ValueError(f"unknown content block type: {block_type!r}")


def _document_text(blocks: tuple[ContentBlock, ...]) -> str:
    parts: list[str] = []
    for block in blocks:
        if isinstance(block, (Heading, Paragraph)):
            parts.append(block.text)
        elif isinstance(block, CodeBlock):
            parts.append(block.code)
        elif isinstance(block, ListBlock):
            parts.extend(block.items)
        elif isinstance(block, Table):
            parts.extend(block.headers)
            parts.extend(cell for row in block.rows for cell in row)
        elif isinstance(block, Reference):
            parts.extend((block.label, block.target))
    return " ".join(parts)
