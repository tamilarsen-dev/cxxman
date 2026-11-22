"""Tests for conservative semantic manual-page projection."""

from pathlib import Path
from io import StringIO

from rich.console import Console

from cxxman.core.document import CodeBlock, Document, Heading, Paragraph, Table
from cxxman.core.manual import project_document
from cxxman.parser.html_parser import parse_html
from cxxman.ui.renderer import render_manual_page


FIXTURES = Path(__file__).parent / "fixtures"
REAL_FIXTURES = FIXTURES / "real"


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
    assert page.purpose == "std::vector is a sequence container."
    sections = {section.title: section.blocks for section in page.sections}
    assert "MEMBERS" in sections
    assert "SEE ALSO" in sections
    assert "Container requirements" in sections["SEE ALSO"][0].text


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
    assert "EXAMPLE" in sections
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
        ("string", "string.html", "class", "<string>", "SYNOPSIS"),
        ("optional", "optional.html", "class", "<optional>", "SYNOPSIS"),
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
    optional_signature = next(
        block
        for block in optional_sections["SYNOPSIS"]
        if isinstance(block, CodeBlock)
    )
    assert (optional_signature.availability or optional_signature.label) == "(since C++17)"

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
    assert unique_sections["RETURNS"] == (
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


def parse_real(topic: str, source_path: str, header: str) -> Document:
    return parse_html(
        (REAL_FIXTURES / f"{topic}.html").read_text(encoding="utf-8"),
        source_url=f"https://en.cppreference.com/cpp/{source_path}",
        canonical_topic=topic if topic != "vector_push_back" else "vector::push_back",
        header=header,
    )


def test_real_vector_card_keeps_purpose_invalidation_members_and_hints() -> None:
    page = project_document(parse_real("vector", "container/vector", "<vector>"))
    sections = {section.title: section for section in page.sections}

    assert page.name == "std::vector"
    assert page.purpose == (
        "std::vector is a sequence container that encapsulates dynamic size arrays."
    )
    assert page.header == "<vector>"
    assert list(sections) == [
        "SYNOPSIS",
        "DESCRIPTION",
        "MEMBERS",
        "WATCH OUT",
        "COMPLEXITY",
        "NOTES",
    ]
    synopsis_hint = sections["SYNOPSIS"].hint
    assert "Example has 20 lines" in synopsis_hint
    assert "Related names:" in synopsis_hint
    assert "Non-member operations:" in synopsis_hint
    declarations = [
        block for block in sections["SYNOPSIS"].blocks if isinstance(block, CodeBlock)
    ]
    assert [block.label for block in declarations] == ["(1)", "(2)"]
    assert declarations[1].availability == "(since C++17)"

    invalidation = next(
        block for block in sections["WATCH OUT"].blocks if isinstance(block, Table)
    )
    assert invalidation.headers == ("Operations", "Invalidated")
    assert invalidation.rows == (
        ("All read only operations", "Never."),
        ("swap, std::swap", "end()"),
        ("clear, operator=, assign", "Always."),
    )
    assert sections["WATCH OUT"].hint.startswith("+6 watch-out")
    member_text = " ".join(
        block.text for block in sections["MEMBERS"].blocks if isinstance(block, Paragraph)
    )
    assert "Element access:" in member_text
    assert "Capacity:" in member_text
    assert "Modifiers:" in member_text
    assert "push_back" in member_text
    assert "public member function" not in " ".join(
        block.text for block in sections["MEMBERS"].blocks if isinstance(block, Paragraph)
    )
    assert "EXAMPLE" not in sections
    assert "Example has 20 lines" in sections["SYNOPSIS"].hint
    assert "--example" in sections["SYNOPSIS"].hint
    assert sections["NOTES"].blocks == (
        Paragraph("In libstdc++, shrink_to_fit() is not available in C++98 mode."),
    )


def test_real_sort_card_selects_overloads_and_hides_long_source_details() -> None:
    page = project_document(parse_real("sort", "algorithm/sort", "<algorithm>"))
    sections = {section.title: section for section in page.sections}
    signatures = [
        block for block in sections["SYNOPSIS"].blocks if isinstance(block, CodeBlock)
    ]

    assert page.purpose == (
        "Sorts the elements in the target range [first, last)."
    )
    assert [signature.label for signature in signatures] == ["(1)", "(2)"]
    assert all("ExecutionPolicy" not in signature.code for signature in signatures)
    assert all(signature.availability == "(constexpr since C++20)" for signature in signatures)
    assert "+2 ExecutionPolicy overloads (C++17)" in sections["SYNOPSIS"].hint
    assert [block.text.split(" — ", 1)[0] for block in sections["PARAMETERS"].blocks] == [
        "first, last"
    ]
    assert "comp" in sections["PARAMETERS"].hint
    assert sections["COMPLEXITY"].blocks == (
        Paragraph("Given \\(\\scriptsize N\\)N as last - first:"),
    )
    assert len(sections["WATCH OUT"].blocks) <= 3
    assert any(
        isinstance(block, Paragraph) and "not guaranteed" in block.text
        for block in sections["WATCH OUT"].blocks
    )
    assert all(
        not isinstance(block, Paragraph) or "std::bad_alloc" not in block.text
        for block in sections["WATCH OUT"].blocks
    )
    assert "EXAMPLE" not in sections
    assert "30 lines" in sections["SYNOPSIS"].hint
    assert "--example" in sections["SYNOPSIS"].hint


def test_real_quick_cards_fit_soft_line_budgets_at_100_columns() -> None:
    cases = (
        (parse_real("sort", "algorithm/sort", "<algorithm>"), 30),
        (parse_real("vector", "container/vector", "<vector>"), 42),
    )
    for document, maximum_lines in cases:
        sink = StringIO()
        console = Console(file=sink, width=100, color_system=None)
        render_manual_page(project_document(document), console=console)
        assert len(sink.getvalue().splitlines()) <= maximum_lines
