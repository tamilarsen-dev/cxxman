"""Accuracy baselines against attributed real cppreference markup."""

from pathlib import Path

from bs4 import BeautifulSoup

from cxxman.core.document import CodeBlock, Heading, Paragraph, Table
from cxxman.parser.html_parser import parse_html


FIXTURES = Path(__file__).parent / "fixtures" / "real"
BASE = "https://en.cppreference.com/w/cpp/"
EXPECTED_FIXTURES = {
    "all_any_none_of",
    "cout",
    "header_algorithm",
    "is_same",
    "optional",
    "same_as",
    "sort",
    "vector",
    "vector_push_back",
}


def parse(topic: str, header: str | None = None):
    return parse_html(
        (FIXTURES / f"{topic}.html").read_text(encoding="utf-8"),
        source_url=BASE + topic,
        canonical_topic=topic,
        header=header,
    )


def test_real_fixture_set_is_complete_trimmed_and_attributed():
    html_files = {path.stem for path in FIXTURES.glob("*.html")}
    assert html_files == EXPECTED_FIXTURES

    attribution = (FIXTURES / "README.txt").read_text(encoding="utf-8")
    assert "cppreference" in attribution
    assert "CC-BY-SA 3.0" in attribution
    assert "GFDL" in attribution

    vector_html = (FIXTURES / "vector.html").read_text(encoding="utf-8")
    assert "t-dcl-begin" in vector_html
    assert 'class="t-dcl"' in vector_html
    assert "t-mark-rev" in vector_html
    assert "t-rev-begin" in vector_html
    assert "t-li1" in vector_html
    assert "t-dsc-begin" in vector_html
    assert "t-lines" in vector_html
    assert "This section is incomplete" in vector_html
    assert "t-navbar" not in vector_html
    assert 'id="toc"' not in vector_html


def test_real_page_identities_and_headers_are_exact():
    expected = {
        "vector": ("std::vector", "<vector>"),
        "sort": ("std::sort", "<algorithm>"),
        "vector_push_back": ("std::vector<T,Allocator>::push_back", "<vector>"),
        "optional": ("std::optional", "<optional>"),
        "is_same": ("std::is_same", "<type_traits>"),
        "same_as": ("std::same_as", "<concepts>"),
        "cout": ("std::cout,std::wcout", "<iostream>"),
    }
    for topic, (title, header) in expected.items():
        document = parse(topic, header)
        assert document.title == title
        assert document.canonical_topic == topic
        assert document.header == header

    header_page = parse("header_algorithm")
    assert "algorithm" in header_page.title.casefold()
    assert header_page.canonical_topic == "header_algorithm"


def test_real_multi_name_source_lists_all_three_exact_algorithms():
    source = BeautifulSoup(
        (FIXTURES / "all_any_none_of.html").read_text(encoding="utf-8"),
        "html.parser",
    )
    assert source.title is not None
    assert source.title.get_text() == "std::all_of,std::any_of,std::none_of"
    assert source.title.get_text().split(",") == [
        "std::all_of",
        "std::any_of",
        "std::none_of",
    ]


def test_real_multi_name_page_parses_rows_with_different_column_counts():
    document = parse("all_any_none_of", "<algorithm>")
    tables = [block for block in document.blocks if isinstance(block, Table)]
    assert tables
    assert all(len(row) == len(table.headers or row) for table in tables for row in table.rows)


def test_real_sort_has_all_four_exact_declarations():
    document = parse("sort", "<algorithm>")
    signatures = [
        block.code
        for block in document.blocks
        if isinstance(block, CodeBlock) and block.code.startswith("template<")
    ]
    assert signatures == [
        "template< class RandomIt > void sort( RandomIt first, RandomIt last );",
        "template< class RandomIt, class Compare > void sort( RandomIt first, RandomIt last, Compare comp );",
        "template< class ExecutionPolicy, class RandomIt > void sort( ExecutionPolicy&& policy, RandomIt first, RandomIt last );",
        "template< class ExecutionPolicy, class RandomIt, class Compare > void sort( ExecutionPolicy&& policy, RandomIt first, RandomIt last, Compare comp );",
    ]


def test_real_vector_projection_includes_iterator_invalidation():
    from cxxman.core.manual import project_document

    sections = {
        section.title: section
        for section in project_document(parse("vector", "<vector>")).sections
    }
    assert "WATCH OUT" in sections
    invalidation = next(
        block for block in sections["WATCH OUT"].blocks if isinstance(block, Table)
    )
    assert invalidation.headers == ("Operations", "Invalidated")
    assert invalidation.rows[0] == ("All read only operations", "Never.")


def test_real_vector_keeps_numbered_t_li1_description():
    document = parse("vector", "<vector>")
    assert Paragraph("1) std::vector is a sequence container that encapsulates dynamic size arrays.") in document.blocks


def test_real_sort_preserves_exact_overload_numbers_and_availability():
    document = parse("sort", "<algorithm>")
    declarations = [block for block in document.blocks if isinstance(block, CodeBlock)]
    actual = [
        (block.code, getattr(block, "label", None), getattr(block, "availability", None))
        for block in declarations
        if block.label is not None
    ]
    assert actual == [
        (
            "template< class RandomIt > void sort( RandomIt first, RandomIt last );",
            "(1)",
            "(constexpr since C++20)",
        ),
        (
            "template< class RandomIt, class Compare > void sort( RandomIt first, RandomIt last, Compare comp );",
            "(2)",
            "(constexpr since C++20)",
        ),
        (
            "template< class ExecutionPolicy, class RandomIt > void sort( ExecutionPolicy&& policy, RandomIt first, RandomIt last );",
            "(3)",
            "(since C++17)",
        ),
        (
            "template< class ExecutionPolicy, class RandomIt, class Compare > void sort( ExecutionPolicy&& policy, RandomIt first, RandomIt last, Compare comp );",
            "(4)",
            "(since C++17)",
        ),
    ]


def test_real_vector_revision_tables_keep_rows_once_and_in_order():
    document = parse("vector", "<vector>")
    tables = [block for block in document.blocks if isinstance(block, Table)]
    flattened_rows = [row for table in tables for row in table.rows]
    assert len(flattened_rows) == len(set(flattened_rows))
    parameter_rows = [row for row in flattened_rows if row[0] in {"T", "Allocator"}]
    assert len(parameter_rows) == 2
    assert "(until C++11)" in parameter_rows[0][2]
    assert "(since C++11)" in parameter_rows[0][2]
    assert "(until C++20)" in parameter_rows[1][2]
    assert "(since C++20)" in parameter_rows[1][2]
    assert "\n" in parameter_rows[0][2]


def test_real_vector_member_names_keep_line_boundaries():
    document = parse("vector", "<vector>")
    text = " ".join(str(block) for block in document.blocks)
    assert "begincbegin" not in text
    assert "endcend" not in text
    assert "assign_range (C++23)" in text


def test_real_vector_filters_maintenance_boxes_and_footnote_arrows():
    document = parse("vector", "<vector>")
    text = " ".join(
        block.text if isinstance(block, (Heading, Paragraph)) else str(block)
        for block in document.blocks
    )
    assert "This section is incomplete" not in text
    assert "↑" not in text

