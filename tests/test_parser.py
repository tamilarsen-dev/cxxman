"""Offline tests for converting HTML to structured documents."""

from pathlib import Path

from cxxman.core.document import CodeBlock, Heading, ListBlock, Paragraph, Reference, Table
from cxxman.parser.html_parser import parse_html

FIXTURES = Path(__file__).parent / "fixtures"


def test_parse_vector_fixture_preserves_order_and_structure() -> None:
    html = (FIXTURES / "vector.html").read_text(encoding="utf-8")

    document = parse_html(
        html,
        source_url="https://en.cppreference.com/w/cpp/container/vector",
        header="<vector>",
    )

    assert document.title == "std::vector"
    assert document.canonical_topic == "vector"
    assert document.header == "<vector>"
    assert isinstance(document.blocks[0], Heading)
    assert isinstance(document.blocks[1], Paragraph)
    assert document.blocks[2] == Reference("Container requirements", "/w/cpp/container")
    assert document.blocks[3] == Heading("Member functions", level=2)
    assert document.blocks[4] == CodeBlock("void push_back(const T& value);", language="cpp")
    assert document.blocks[5] == ListBlock(("push_back", "pop_back"))
    assert document.blocks[6] == ListBlock(("construct", "initialize"), ordered=True)
    assert document.blocks[7] == Table(
        rows=(("push_back", "Amortized constant"), ("at", "Constant")),
        headers=("Operation", "Complexity"),
    )
    assert all("navigation" not in getattr(block, "text", "") for block in document.blocks)


def test_parser_falls_back_gracefully_when_content_container_is_missing() -> None:
    document = parse_html(
        "<html><body><h1>std::sort</h1><p>Sorts a range.</p></body></html>",
        source_url="https://example.test/sort",
    )

    assert document.title == "std::sort"
    assert document.canonical_topic == "sort"
    assert document.blocks == (Heading("std::sort", level=1), Paragraph("Sorts a range."))
