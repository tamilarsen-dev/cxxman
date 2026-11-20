"""Tests for conservative semantic manual-page projection."""

from pathlib import Path

from cxxman.core.document import CodeBlock, Document, Heading, Paragraph
from cxxman.core.manual import project_document
from cxxman.parser.html_parser import parse_html


FIXTURES = Path(__file__).parent / "fixtures"


def test_project_vector_keeps_supported_sections_and_source_metadata() -> None:
    document = parse_html(
        (FIXTURES / "vector.html").read_text(encoding="utf-8"),
        source_url="https://en.cppreference.com/w/cpp/container/vector",
        header="<vector>",
    )

    page = project_document(document)

    assert page.entity_kind == "class"
    assert page.header == "<vector>"
    assert page.source_url.endswith("/vector")
    assert [section.title for section in page.sections] == [
        "DESCRIPTION",
        "MEMBER FUNCTIONS",
        "SEE ALSO",
    ]
    member_blocks = page.sections[1].blocks
    assert any(
        isinstance(block, CodeBlock) and "push_back" in block.code
        for block in member_blocks
    )


def test_project_push_back_preserves_overloads_and_dynamic_sections() -> None:
    document = parse_html(
        (FIXTURES / "vector_push_back.html").read_text(encoding="utf-8"),
        source_url="https://en.cppreference.com/w/cpp/container/vector/push_back",
        canonical_topic="vector::push_back",
        header="<vector>",
    )

    page = project_document(document)
    sections = {section.title: section.blocks for section in page.sections}

    assert page.entity_kind == "member function"
    signatures = [
        block.code
        for block in sections["SYNOPSIS"]
        if isinstance(block, CodeBlock)
    ]
    assert signatures == [
        "void push_back( const T& value );",
        "void push_back( T&& value );",
    ]
    assert "PARAMETERS" in sections
    assert "COMPLEXITY" in sections
    assert "EXAMPLES" in sections
    assert "EXCEPTIONS" not in sections


def test_projection_omits_empty_and_unknown_sections_without_mutation() -> None:
    document = Document(
        "id",
        "std::thing",
        "thing",
        "https://example.test/thing",
        (
            Heading("std::thing", level=1),
            Heading("Exceptions"),
            Heading("Compiler support"),
            Paragraph("Toolchain-only content."),
            Heading("Description"),
            Paragraph("A sourced description."),
        ),
    )

    page = project_document(document)

    assert page.entity_kind == "unknown"
    assert [section.title for section in page.sections] == ["DESCRIPTION"]
    assert page.sections[0].blocks == (Paragraph("A sourced description."),)
    assert len(document.blocks) == 6


def test_projection_covers_algorithm_string_and_optional_fixtures() -> None:
    cases = (
        ("sort", "sort.html", "algorithm", "<algorithm>", "SYNOPSIS"),
        ("unique", "unique.html", "algorithm", "<algorithm>", "SYNOPSIS"),
        ("string", "string.html", "class", "<string>", "TEMPLATE PARAMETERS"),
        ("optional", "optional.html", "class", "<optional>", "TEMPLATE PARAMETERS"),
    )

    for topic, fixture, expected_kind, header, required_section in cases:
        document = parse_html(
            (FIXTURES / fixture).read_text(encoding="utf-8"),
            source_url=f"https://en.cppreference.com/cpp/{topic}",
            canonical_topic=topic,
            header=header,
        )
        page = project_document(document)

        assert page.entity_kind == expected_kind
        assert page.header == header
        assert page.source_url.endswith(topic)
        assert required_section in {section.title for section in page.sections}

    optional_document = parse_html(
        (FIXTURES / "optional.html").read_text(encoding="utf-8"),
        source_url="https://en.cppreference.com/w/cpp/utility/optional",
        canonical_topic="optional",
        header="<optional>",
    )
    optional_sections = {
        section.title: section.blocks for section in project_document(optional_document).sections
    }
    assert optional_sections["STANDARDS"] == (Paragraph("Available since C++17."),)

    sort_document = parse_html(
        (FIXTURES / "sort.html").read_text(encoding="utf-8"),
        source_url="https://en.cppreference.com/w/cpp/algorithm/sort",
        canonical_topic="sort",
        header="<algorithm>",
    )
    sort_sections = {section.title: section.blocks for section in project_document(sort_document).sections}
    signatures = [block.code for block in sort_sections["SYNOPSIS"] if isinstance(block, CodeBlock)]
    assert signatures == [
        "template< class RandomIt > void sort( RandomIt first, RandomIt last );",
        "template< class RandomIt, class Compare > void sort( RandomIt first, RandomIt last, Compare comp );",
    ]
    assert sort_sections["COMPLEXITY"] == (Paragraph("O(N log N) comparisons."),)

    unique_document = parse_html(
        (FIXTURES / "unique.html").read_text(encoding="utf-8"),
        source_url="https://en.cppreference.com/w/cpp/algorithm/unique",
        canonical_topic="unique",
        header="<algorithm>",
    )
    unique_sections = {
        section.title: section.blocks for section in project_document(unique_document).sections
    }
    unique_signatures = [
        block.code for block in unique_sections["SYNOPSIS"] if isinstance(block, CodeBlock)
    ]
    assert len(unique_signatures) == 2
    assert all("ForwardIt unique(" in signature for signature in unique_signatures)
    assert unique_sections["RETURN VALUE"] == (
        Paragraph("Past-the-end iterator for the new logical end of the range."),
    )

    class_signatures = {}
    for topic, fixture, header in (
        ("string", "string.html", "<string>"),
        ("optional", "optional.html", "<optional>"),
    ):
        document = parse_html(
            (FIXTURES / fixture).read_text(encoding="utf-8"),
            source_url=f"https://en.cppreference.com/cpp/{topic}",
            canonical_topic=topic,
            header=header,
        )
        sections = {section.title: section.blocks for section in project_document(document).sections}
        class_signatures[topic] = [
            block.code for block in sections["SYNOPSIS"] if isinstance(block, CodeBlock)
        ]
    assert class_signatures["string"] == [
        "template< class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT> > class basic_string;"
    ]
    assert class_signatures["optional"] == ["template< class T > class optional;"]
