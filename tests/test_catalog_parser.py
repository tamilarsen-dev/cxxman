"""Tests for extraction from real cppreference catalog markup."""

from pathlib import Path

from cxxman.parser.catalog_parser import parse_header_index


FIXTURE_DIR = Path(__file__).parent / "fixtures" / "real"


def test_header_index_retains_source_names_purpose_and_group() -> None:
    html = (FIXTURE_DIR / "header_algorithm.html").read_text(encoding="utf-8")

    entries = parse_header_index(html, header="<algorithm>")
    all_of = next(entry for entry in entries if entry.topic == "all_of")

    assert all_of.aliases == ("any_of", "none_of")
    assert all_of.source_path == "algorithm/all_any_none_of"
    assert all_of.entity_kind == "algorithm"
    assert all_of.purpose == "checks if a predicate is true for all, any or none of the elements in a range"
    assert all_of.group == "Functions / Non-modifying sequence operations"


def test_header_index_excludes_non_cpp_and_unlinked_rows() -> None:
    html = """<div id="mw-content-text"><div class="mw-parser-output">
    <h2>Functions</h2><table class="t-dsc-begin"><tbody>
    <tr class="t-dsc"><td><a href="/c/math">sin</a></td><td>bad row</td></tr>
    <tr class="t-dsc"><td><a href="/cpp/compiler_support/feature">feature</a></td><td>not library</td></tr>
    <tr class="t-dsc"><td>missing link</td><td>bad row</td></tr>
    </tbody></table></div></div>"""

    assert parse_header_index(html, header="<cmath>") == ()
