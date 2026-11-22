"""Typed structures for parsed C++ reference documents."""

from __future__ import annotations

from dataclasses import dataclass
from typing import TypeAlias


@dataclass(frozen=True, slots=True)
class Heading:
    """A section heading, with its HTML-style level from 1 to 6."""

    text: str
    level: int = 2

    def __post_init__(self) -> None:
        if not 1 <= self.level <= 6:
            raise ValueError("heading level must be between 1 and 6")


@dataclass(frozen=True, slots=True)
class Paragraph:
    """A prose paragraph."""

    text: str


@dataclass(frozen=True, slots=True)
class CodeBlock:
    """A code example or function signature."""

    code: str
    language: str | None = None
    label: str | None = None
    availability: str | None = None


@dataclass(frozen=True, slots=True)
class ListBlock:
    """An ordered or unordered list of text items."""

    items: tuple[str, ...]
    ordered: bool = False


@dataclass(frozen=True, slots=True)
class Table:
    """A table with optional column headings and rectangular rows."""

    rows: tuple[tuple[str, ...], ...]
    headers: tuple[str, ...] = ()

    def __post_init__(self) -> None:
        width = len(self.headers) if self.headers else (len(self.rows[0]) if self.rows else 0)
        if any(len(row) != width for row in self.rows):
            raise ValueError("all table rows must have the same number of columns")


@dataclass(frozen=True, slots=True)
class Reference:
    """A link to another reference page or an external resource."""

    label: str
    target: str


ContentBlock: TypeAlias = Heading | Paragraph | CodeBlock | ListBlock | Table | Reference


@dataclass(frozen=True, slots=True)
class Document:
    """A reference page and its ordered, structured content."""

    identifier: str
    title: str
    canonical_topic: str
    source_url: str
    blocks: tuple[ContentBlock, ...]
    header: str | None = None
    updated_at: str | None = None
    purpose: str | None = None
    aliases: tuple[str, ...] = ()
