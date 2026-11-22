"""Tests for the stored-page coverage sweep."""

from datetime import date
from pathlib import Path

from cxxman.core.document import CodeBlock, Document, Paragraph, Table
from cxxman.parser.html_parser import parse_html
from cxxman.storage.database import Database
from tools.validate_coverage import format_report, inspect_document, validate_database


def _document(topic: str, blocks) -> Document:
    return Document(
        identifier=f"cppref/{topic}",
        title=f"std::{topic}",
        canonical_topic=topic,
        source_url=f"https://en.cppreference.com/cpp/{topic}",
        blocks=tuple(blocks),
    )


def test_inspector_reports_missing_purpose_and_synopsis() -> None:
    findings = inspect_document(_document("sort", ()))

    assert "missing_purpose" in findings
    assert "missing_synopsis" in findings


def test_inspector_reports_duplicate_rows_and_concatenated_member_names() -> None:
    document = _document(
        "sort",
        (
            Paragraph("Sorts a range."),
            CodeBlock("void sort();"),
            Table((
                ("begin", "iterator"),
                ("begin", "iterator"),
                ("begincbegin", "member names"),
            )),
        ),
    )

    findings = inspect_document(document)

    assert findings["duplicate_rows"] == "1 duplicated table row(s)"
    assert findings["concatenated_names"] == "begincbegin"
    assert "missing_synopsis" not in findings


def test_real_vector_fixture_passes_current_synopsis_name_and_budget_checks() -> None:
    fixture = Path(__file__).parent / "fixtures" / "real" / "vector.html"
    document = parse_html(
        fixture.read_text(encoding="utf-8"),
        source_url="https://en.cppreference.com/cpp/container/vector",
        identifier="cppref/container/vector",
        canonical_topic="vector",
        header="<vector>",
    )

    findings = inspect_document(document)

    assert "missing_purpose" not in findings
    assert "missing_synopsis" not in findings
    assert "concatenated_names" not in findings
    assert "over_budget" not in findings


def test_inspector_checks_rendered_function_card_budget() -> None:
    declarations = tuple(CodeBlock("void sort();") for _ in range(35))
    findings = inspect_document(
        _document("sort", (Paragraph("Sorts a range."), *declarations))
    )

    assert findings["over_budget"].endswith("budget is 30")


def test_database_sweep_is_read_only_and_report_lists_parse_failures(tmp_path) -> None:
    database_path = tmp_path / "docs.sqlite3"
    with Database(database_path) as database:
        database.save_document(_document("sort", (Paragraph("Sorts a range."),)))
        with database._connection:
            database._connection.execute(
                "UPDATE documents SET content_json = ?, format_version = 1 "
                "WHERE canonical_topic = ?",
                ("not-json", "sort"),
            )

    modified_before = database_path.stat().st_mtime_ns
    result = validate_database(database_path)
    report = format_report(result, run_date=date(2026, 11, 22))
    modified_after = database_path.stat().st_mtime_ns

    assert result.pages_scanned == 1
    assert len(result.parse_failures) == 1
    assert len(result.stale_records) == 1
    assert modified_after == modified_before
    assert "Pages scanned: 1" in report
    assert "`sort` — JSONDecodeError" in report

