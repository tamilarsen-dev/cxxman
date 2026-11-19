"""Search result types and local document search behavior."""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from cxxman.storage.database import Database


@dataclass(frozen=True, slots=True)
class SearchResult:
    """Metadata for a matching page; content is loaded only when opened."""

    identifier: str
    title: str
    canonical_topic: str


def search_documents(database: Database, query: str, limit: int = 20) -> list[SearchResult]:
    """Search the local SQLite index and return a bounded result list."""
    return database.search(query, limit=limit)
