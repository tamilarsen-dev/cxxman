"""Tests for readable terminal rendering of document blocks."""

from contextlib import contextmanager
from io import StringIO

from rich.console import Console

from cxxman.core.document import (
    CodeBlock,
    Document,
    Heading,
    ListBlock,
    Paragraph,
    Reference,
    Table,
)
from cxxman.core.manual import ManualPage, ManualSection
from cxxman.ui.renderer import render_document, render_manual_page


def test_render_document_covers_supported_blocks() -> None:
    sink = StringIO()
    console = Console(file=sink, width=60, color_system=None, highlight=False)
    document = Document(
        identifier="cpp/vector",
        title="std::vector",
        canonical_topic="vector",
        source_url="https://example.test/vector",
        header="<vector>",
        blocks=(
            Heading("Member functions"),
            Paragraph("A sequence container."),
            CodeBlock("void push_back(const T& value);", language="cpp"),
            ListBlock(("push_back", "pop_back")),
            ListBlock(("first", "second"), ordered=True),
            Table((("push_back", "Amortized constant"),), headers=("Operation", "Complexity")),
            Reference("Container requirements", "cpp/container"),
        ),
    )

    render_document(document, console=console)
    output = sink.getvalue()

    for expected in (
        "std::vector",
        "<vector>",
        "Member functions",
        "A sequence container.",
        "void push_back(const T& value);",
        "• push_back",
        "1. first",
        "Operation",
        "Amortized constant",
        "Container requirements (cpp/container)",
    ):
        assert expected in output


def test_render_manual_page_shows_only_present_sections_and_source() -> None:
    sink = StringIO()
    console = Console(file=sink, width=60, color_system=None, highlight=False)
    page = ManualPage(
        "std::vector::push_back",
        "member function",
        "<vector>",
        "https://example.test/vector/push_back",
        (
            ManualSection(
                "SYNOPSIS",
                (CodeBlock("void push_back(const T& value);", language="cpp"),),
            ),
            ManualSection("COMPLEXITY", (Paragraph("Amortized constant."),)),
            ManualSection("FUTURE SECTION", (Paragraph("Unknown sections render safely."),)),
        ),
    )

    render_manual_page(page, console=console)

    output = sink.getvalue()
    assert "SYNOPSIS" in output
    assert "COMPLEXITY" in output
    assert "FUTURE SECTION" in output
    assert "Unknown sections render safely." in output
    assert "PARAMETERS" not in output
    assert "<vector>" in output
    assert "REFERENCES" in output
    assert "https://example.test/vector/push_back" in output


def test_render_manual_page_merges_source_into_existing_references_section() -> None:
    sink = StringIO()
    console = Console(file=sink, width=60, color_system=None, highlight=False)
    page = ManualPage(
        "std::vector",
        "class",
        "<vector>",
        "https://example.test/vector",
        (
            ManualSection(
                "REFERENCES",
                (Reference("Container requirements", "container"),),
            ),
        ),
    )

    render_manual_page(page, console=console)

    output = sink.getvalue()
    assert output.count("REFERENCES") == 1
    assert "Container requirements (container)" in output
    assert "https://example.test/vector" in output


def test_manual_page_cpp_syntax_is_highlighted_when_color_is_enabled() -> None:
    sink = StringIO()
    console = Console(file=sink, width=60, force_terminal=True, color_system="truecolor")
    page = ManualPage(
        "std::vector",
        "class",
        "<vector>",
        "https://example.test/vector",
        (
            ManualSection(
                "SYNOPSIS",
                (CodeBlock("int answer = 42;", language="cpp"),),
            ),
        ),
    )

    render_manual_page(page, console=console)

    output = sink.getvalue()
    assert "answer" in output
    assert output.count("\x1b[") >= 6


def test_render_pagination_is_ignored_for_piped_output() -> None:
    sink = StringIO()
    console = Console(file=sink, width=40, color_system=None)
    document = Document("id", "Title", "topic", "url", (Paragraph("Text"),))

    render_document(document, console=console, paginate=True)

    assert "Text" in sink.getvalue()


def test_render_uses_pager_for_terminal_output(monkeypatch) -> None:
    sink = StringIO()
    console = Console(file=sink, width=40, color_system=None, force_terminal=True)
    document = Document("id", "Title", "topic", "url", (Paragraph("Text"),))
    pager_styles: list[bool] = []

    @contextmanager
    def fake_pager(*, styles: bool = False):
        pager_styles.append(styles)
        yield

    monkeypatch.setattr(console, "pager", fake_pager)
    render_document(document, console=console, paginate=True)

    assert pager_styles == [True]
