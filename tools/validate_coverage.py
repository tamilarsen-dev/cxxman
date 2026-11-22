r"""Validate stored manual cards and write a coverage report.

Run with ``.venv\Scripts\python.exe tools\validate_coverage.py``. The
database is opened read-only; use ``--database`` to select a corpus explicitly.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import date
from io import StringIO
from pathlib import Path
import re
import sqlite3
from typing import Iterable

from rich.console import Console

from cxxman.core.document import CodeBlock, Document, Table
from cxxman.core.catalog import SOURCE_PAGES
from cxxman.core.manual import (
    ManualPage,
    _looks_like_declaration,
    _split_sections,
    project_document,
)
from cxxman.storage.database import (
    _DOCUMENT_FORMAT_VERSION,
    _deserialize_document,
    default_database_path,
)
from cxxman.ui.renderer import render_manual_page


_CONCATENATED_MEMBER_NAMES = re.compile(
    r"(?:begin|cbegin|end|cend)(?:begin|cbegin|end|cend)", re.IGNORECASE
)
_BUDGETS = {"algorithm": 30, "member function": 30, "class": 42}


@dataclass(frozen=True, slots=True)
class PageFinding:
    """One validation finding associated with a stored page."""

    topic: str
    detail: str


@dataclass(frozen=True, slots=True)
class CoverageResult:
    """Counts and page-level findings from one stored-corpus sweep."""

    pages_scanned: int
    width: int
    parse_failures: tuple[PageFinding, ...]
    stale_records: tuple[PageFinding, ...]
    missing_purpose: tuple[PageFinding, ...]
    missing_synopsis: tuple[PageFinding, ...]
    duplicate_rows: tuple[PageFinding, ...]
    concatenated_names: tuple[PageFinding, ...]
    over_budget: tuple[PageFinding, ...]
    catalog_name_count: int
    catalog_route_count: int
    missing_catalog_routes: tuple[str, ...]
    stored_outside_catalog: tuple[str, ...]

    @property
    def finding_count(self) -> int:
        """Return the number of page/category findings, allowing overlap."""
        return sum(
            len(items)
            for items in (
                self.parse_failures,
                self.stale_records,
                self.missing_synopsis,
                self.duplicate_rows,
                self.concatenated_names,
                self.over_budget,
            )
        ) + len(self.missing_catalog_routes) + len(self.stored_outside_catalog)


def inspect_document(document: Document, *, width: int = 100) -> dict[str, str]:
    """Return validation findings for one parsed document.

    Concatenated-name detection targets the known cppreference member-name
    failure where ``begin``/``end`` variants were joined without separators.
    Card budgets are checked only for the function and class kinds specified
    by the quick-reference spec.
    """
    page = project_document(document)
    findings: dict[str, str] = {}
    if not page.purpose:
        findings["missing_purpose"] = "no source purpose is available for the NAME line"

    synopsis = next((section for section in page.sections if section.title == "SYNOPSIS"), None)
    before_sections, source_sections = _split_sections(document)
    source_declarations = [
        block for block in before_sections if _looks_like_declaration(block)
    ]
    source_declarations.extend(
        block
        for block in source_sections.get("SYNOPSIS", ())
        if _looks_like_declaration(block)
    )
    if source_declarations and (synopsis is None or not any(
        isinstance(block, CodeBlock) and block.code.strip() for block in synopsis.blocks
    )):
        findings["missing_synopsis"] = "no declaration is present in SYNOPSIS"

    duplicate_count = _duplicate_table_rows(document)
    if duplicate_count:
        findings["duplicate_rows"] = f"{duplicate_count} duplicated table row(s)"

    concatenated = _concatenated_names(document)
    if concatenated:
        findings["concatenated_names"] = ", ".join(concatenated)

    budget = _BUDGETS.get(page.entity_kind)
    if budget is not None:
        rendered_lines = _rendered_line_count(page, width)
        if rendered_lines > budget:
            findings["over_budget"] = f"{rendered_lines} rendered lines; budget is {budget}"
    return findings


def validate_database(database_path: Path, *, width: int = 100) -> CoverageResult:
    """Sweep all stored pages one at a time through a read-only connection."""
    if width < 20:
        raise ValueError("terminal width must be at least 20 columns")
    uri = f"{database_path.expanduser().resolve().as_uri()}?mode=ro"
    connection = sqlite3.connect(uri, uri=True)
    connection.row_factory = sqlite3.Row
    catalog_pages = tuple(SOURCE_PAGES.values())
    catalog_routes = {page.identifier for page in catalog_pages}
    stored_identifiers: set[str] = set()
    findings: dict[str, list[PageFinding]] = {
        "parse_failures": [],
        "stale_records": [],
        "missing_purpose": [],
        "missing_synopsis": [],
        "duplicate_rows": [],
        "concatenated_names": [],
        "over_budget": [],
    }
    pages_scanned = 0
    try:
        cursor = connection.execute(
            """SELECT identifier, title, canonical_topic, source_url, header,
                      updated_at, content_json, purpose, aliases_json, format_version
               FROM documents
               ORDER BY canonical_topic COLLATE NOCASE, identifier"""
        )
        for row in cursor:
            topic = str(row["canonical_topic"])
            stored_identifiers.add(str(row["identifier"]))
            if row["format_version"] < _DOCUMENT_FORMAT_VERSION:
                findings["stale_records"].append(
                    PageFinding(
                        topic,
                        f"stored format {row['format_version']}; current format is "
                        f"{_DOCUMENT_FORMAT_VERSION}; run 'cxxman update {topic}'",
                    )
                )
            try:
                document = _deserialize_document(row)
                page_findings = inspect_document(document, width=width)
            except Exception as error:  # per-page validation boundary
                findings["parse_failures"].append(
                    PageFinding(topic, f"{type(error).__name__}: {error}")
                )
                pages_scanned += 1
                continue
            pages_scanned += 1
            for category, detail in page_findings.items():
                findings[category].append(PageFinding(topic, detail))
    finally:
        connection.close()

    return CoverageResult(
        pages_scanned=pages_scanned,
        width=width,
        catalog_name_count=len(catalog_pages),
        catalog_route_count=len(catalog_routes),
        missing_catalog_routes=tuple(sorted(catalog_routes - stored_identifiers)),
        stored_outside_catalog=tuple(sorted(stored_identifiers - catalog_routes)),
        **{category: tuple(items) for category, items in findings.items()},
    )


def format_report(result: CoverageResult, *, run_date: date | None = None) -> str:
    """Format a deterministic Markdown report for the validated corpus."""
    completed = run_date or date.today()
    lines = [
        "# Stored-page coverage report",
        "",
        f"Run date: {completed.isoformat()}",
        f"Pages scanned: {result.pages_scanned}",
        f"Rendered width: {result.width} columns",
        f"Catalog names: {result.catalog_name_count}",
        f"Unique catalog routes: {result.catalog_route_count}",
        "",
        "| Check | Pages with findings |",
        "|---|---:|",
        f"| Parse failures | {len(result.parse_failures)} |",
        f"| Stored pages needing refresh | {len(result.stale_records)} |",
        f"| NAME without source purpose (informational) | {len(result.missing_purpose)} |",
        f"| Missing SYNOPSIS | {len(result.missing_synopsis)} |",
        f"| Duplicate table rows | {len(result.duplicate_rows)} |",
        f"| Concatenated member names | {len(result.concatenated_names)} |",
        f"| Over card budget | {len(result.over_budget)} |",
        f"| Catalog routes missing locally | {len(result.missing_catalog_routes)} |",
        f"| Stored pages outside catalog | {len(result.stored_outside_catalog)} |",
        "",
        "Budgets: 30 rendered lines for algorithms/member functions and 42 for classes.",
        "Other entity kinds have no line budget in the current spec.",
        "",
        "Purpose is omitted when cppreference provides no source sentence; this is informational.",
        "Catalog-route counts compare distinct source URLs, since catalog names may share a page.",
    ]
    categories: Iterable[tuple[str, tuple[PageFinding, ...]]] = (
        ("Parse failures", result.parse_failures),
        ("Stored pages needing refresh", result.stale_records),
        ("NAME shown without source purpose (informational)", result.missing_purpose),
        ("Pages missing SYNOPSIS", result.missing_synopsis),
        ("Pages with duplicate table rows", result.duplicate_rows),
        ("Pages with concatenated member names", result.concatenated_names),
        ("Pages over card budget", result.over_budget),
        (
            "Catalog routes missing locally",
            tuple(PageFinding(route, "not stored") for route in result.missing_catalog_routes),
        ),
        (
            "Stored pages outside catalog",
            tuple(PageFinding(route, "not in generated catalog") for route in result.stored_outside_catalog),
        ),
    )
    for title, items in categories:
        lines.extend(("", f"## {title}", ""))
        if not items:
            lines.append("None.")
            continue
        for finding in items:
            lines.append(f"- `{finding.topic}` — {finding.detail}")
    return "\n".join(lines) + "\n"


def _duplicate_table_rows(document: Document) -> int:
    duplicates = 0
    for block in document.blocks:
        if not isinstance(block, Table):
            continue
        seen: set[tuple[str, ...]] = set()
        for row in block.rows:
            normalized = tuple(" ".join(cell.split()).casefold() for cell in row)
            if not any(normalized):
                continue
            if len(normalized) > 1 and not any(normalized[1:]):
                continue
            if normalized in seen:
                duplicates += 1
            else:
                seen.add(normalized)
    return duplicates


def _concatenated_names(document: Document) -> tuple[str, ...]:
    found: list[str] = []
    for block in document.blocks:
        if isinstance(block, Table):
            for row in block.rows:
                for cell in row:
                    if _CONCATENATED_MEMBER_NAMES.search(cell) and cell not in found:
                        found.append(cell)
    return tuple(found)


def _rendered_line_count(page: ManualPage, width: int) -> int:
    buffer = StringIO()
    console = Console(file=buffer, width=width, force_terminal=False, color_system=None)
    render_manual_page(page, console=console, paginate=False)
    return len(buffer.getvalue().splitlines())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, default=default_database_path())
    parser.add_argument("--report", type=Path, default=Path("docs/coverage-report.md"))
    parser.add_argument("--width", type=int, default=100)
    args = parser.parse_args()
    try:
        result = validate_database(args.database, width=args.width)
        report = format_report(result)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(report, encoding="utf-8")
    except (OSError, sqlite3.Error, ValueError) as error:
        print(f"coverage validation failed: {error}")
        return 2
    print(
        f"Validated {result.pages_scanned} stored page(s); "
        f"{result.finding_count} page/category finding(s). Report: {args.report}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
