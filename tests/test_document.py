"""Tests for the structured document model."""

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


def test_document_holds_metadata_and_ordered_content() -> None:
    blocks = (
        Heading("std::vector", level=1),
        Paragraph("A dynamically sized sequence container."),
        CodeBlock("template<class T> class vector;", language="cpp"),
        ListBlock(("push_back", "pop_back")),
        Table((("O(1)", "constant"),), headers=("Operation", "Complexity")),
        Reference("Allocator requirements", "allocator"),
    )
    document = Document(
        identifier="cpp/vector",
        title="std::vector",
        canonical_topic="vector",
        source_url="https://example.test/vector",
        blocks=blocks,
        header="<vector>",
        updated_at="2026-11-19T00:00:00Z",
    )

    assert document.blocks == blocks
    assert document.header == "<vector>"
    assert document.blocks[2].code == "template<class T> class vector;"


def test_heading_rejects_invalid_level() -> None:
    with pytest.raises(ValueError, match="between 1 and 6"):
        Heading("Invalid", level=7)


def test_table_rejects_rows_with_different_widths() -> None:
    with pytest.raises(ValueError, match="same number of columns"):
        Table(rows=(("a", "b"), ("c",)))
