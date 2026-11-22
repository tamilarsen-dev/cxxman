"""Tests for the initial command-line interface."""

import pytest

from cxxman import __version__
from cxxman.cli import main


def test_help(capsys: pytest.CaptureFixture[str]) -> None:
    with pytest.raises(SystemExit) as exc_info:
        main(["--help"])

    assert exc_info.value.code == 0
    output = capsys.readouterr().out
    assert "usage: cxxman" in output
    assert "cxxman topics" in output
    assert "cxxman update vector::push_back" in output
    assert "--full" in output
    assert "--example" in output


def test_topics_lists_downloadable_pages(capsys: pytest.CaptureFixture[str]) -> None:
    assert main(["topics"]) == 0

    output = capsys.readouterr().out
    assert "Topics available to download" in output
    assert "vector::push_back" in output
    assert "<vector>" in output


def test_header_lists_catalog_entries(capsys: pytest.CaptureFixture[str]) -> None:
    assert main(["header", "<vector>"]) == 0

    output = capsys.readouterr().out
    assert "<vector>" in output
    assert "vector" in output


def test_header_name_lists_entities_while_entity_name_keeps_precedence(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from cxxman.storage.database import Database

    monkeypatch.setenv("CXXMAN_DB_PATH", str(tmp_path / "docs.sqlite3"))
    with Database(tmp_path / "docs.sqlite3") as database:
        from cxxman.core.document import Document, Paragraph

        database.save_document(
            Document("id", "std::vector", "vector", "url", (Paragraph("sequence"),))
        )

    assert main(["<vector>"]) == 0
    assert "vector" in capsys.readouterr().out
    assert main(["algorithm"]) == 0
    assert "algorithm" in capsys.readouterr().out
    assert main(["vector"]) == 0
    assert "std::vector" in capsys.readouterr().out


def test_no_arguments_show_quick_start(capsys: pytest.CaptureFixture[str]) -> None:
    assert main([]) == 0

    output = capsys.readouterr().out
    assert "Quick start:" in output
    assert "cxxman topics" in output
    assert "cxxman update vector" in output


def test_version(capsys: pytest.CaptureFixture[str]) -> None:
    with pytest.raises(SystemExit) as exc_info:
        main(["--version"])

    assert exc_info.value.code == 0
    assert f"cxxman {__version__}" in capsys.readouterr().out


def test_check_updates_reports_new_release(capsys, monkeypatch: pytest.MonkeyPatch) -> None:
    from cxxman.core.app_updates import LatestRelease

    monkeypatch.setattr(
        "cxxman.core.app_updates.fetch_latest_release",
        lambda: LatestRelease(
            "v9.0.0", "9.0.0", "https://github.com/tamilarsen-dev/cxxman/releases/tag/v9.0.0", {}
        ),
    )
    assert main(["check-updates"]) == 0
    output = capsys.readouterr().out
    assert "Update available: cxxman 9.0.0" in output
    assert "cxxman self-update" in output


def test_self_update_explains_python_managed_installation(capsys, monkeypatch) -> None:
    import sys

    monkeypatch.setattr(sys, "frozen", False, raising=False)
    assert main(["self-update"]) == 1
    assert "managed by Python" in capsys.readouterr().err


def test_topic_reports_not_available(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("CXXMAN_DB_PATH", str(tmp_path / "docs.sqlite3"))
    exit_code = main(["vector"])

    captured = capsys.readouterr()
    assert exit_code == 1
    assert "'vector' is not downloaded yet" in captured.err
    assert "cxxman update vector" in captured.err


def test_unknown_topic_suggests_supported_topic(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("CXXMAN_DB_PATH", str(tmp_path / "docs.sqlite3"))

    assert main(["vectro"]) == 1

    error = capsys.readouterr().err
    assert "'vector' (cxxman vector)" in error
    assert "cxxman topics" in error


def test_search_explains_empty_cache_and_no_match(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from cxxman.core.document import Document, Paragraph
    from cxxman.storage.database import Database

    database_path = tmp_path / "docs.sqlite3"
    monkeypatch.setenv("CXXMAN_DB_PATH", str(database_path))

    assert main(["search", "vector"]) == 0
    assert "No pages are cached yet" in capsys.readouterr().out

    with Database(database_path) as database:
        database.save_document(
            Document("id", "std::vector", "vector", "url", (Paragraph("container"),))
        )
    assert main(["search", "deque"]) == 0
    assert "Search only checks pages you've downloaded" in capsys.readouterr().out


def test_search_list_and_topic_read_from_local_database(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from cxxman.core.document import CodeBlock, Document, Paragraph
    from cxxman.storage.database import Database

    database_path = tmp_path / "docs.sqlite3"
    monkeypatch.setenv("CXXMAN_DB_PATH", str(database_path))
    with Database(database_path) as database:
        database.save_document(
            Document(
                "cppref/vector",
                "std::vector",
                "vector",
                "https://example.test/vector",
                (Paragraph("A sequence container."),),
            )
        )
        database.save_document(
            Document(
                "cppref/container/vector/push_back",
                "std::vector<T,Allocator>::push_back",
                "vector::push_back",
                "https://example.test/vector/push_back",
                (CodeBlock("void push_back(const T& value);", language="cpp"),),
            )
        )

    assert main(["list"]) == 0
    assert "vector  std::vector" in capsys.readouterr().out

    assert main(["search", "sequence"]) == 0
    assert "vector  std::vector" in capsys.readouterr().out

    assert main(["vector"]) == 0
    output = capsys.readouterr().out
    assert "std::vector" in output
    assert "A sequence container." in output

    assert main(["std::vector::push_back"]) == 0
    assert "std::vector<T,Allocator>::push_back" in capsys.readouterr().out


def test_manual_page_color_option_controls_terminal_styles(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from cxxman.core.document import CodeBlock, Document
    from cxxman.storage.database import Database

    database_path = tmp_path / "docs.sqlite3"
    monkeypatch.setenv("CXXMAN_DB_PATH", str(database_path))
    with Database(database_path) as database:
        database.save_document(
            Document(
                "cppref/container/vector",
                "std::vector",
                "vector",
                "https://example.test/vector",
                (CodeBlock("int answer = 42;", language="cpp"),),
                header="<vector>",
            )
        )

    assert main(["--color", "always", "vector"]) == 0
    colored_output = capsys.readouterr().out
    assert "\x1b[" in colored_output
    assert "answer" in colored_output

    assert main(["vector", "--color", "never"]) == 0
    plain_output = capsys.readouterr().out
    assert "\x1b[" not in plain_output
    assert "answer" in plain_output


def test_full_and_example_options_show_the_selected_source_content(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from pathlib import Path

    from cxxman.parser.html_parser import parse_html
    from cxxman.storage.database import Database

    fixture_path = Path(__file__).parent / "fixtures" / "real" / "sort.html"
    document = parse_html(
        fixture_path.read_text(encoding="utf-8"),
        source_url="https://en.cppreference.com/cpp/algorithm/sort",
        identifier="cppref/algorithm/sort",
        canonical_topic="sort",
        header="<algorithm>",
    )
    database_path = tmp_path / "docs.sqlite3"
    monkeypatch.setenv("CXXMAN_DB_PATH", str(database_path))
    with Database(database_path) as database:
        database.save_document(document)

    assert main(["sort", "--full", "--color", "never"]) == 0
    full_output = capsys.readouterr().out
    assert "Possible implementation" in full_output
    assert "Defect reports" in full_output

    assert main(["sort", "--example", "--color", "never"]) == 0
    example_output = capsys.readouterr().out
    assert "Examples for std::sort" in example_output
    assert "std::sort(s.begin(), s.end());" in example_output
    assert "template< class RandomIt >" not in example_output


def test_example_option_reports_when_no_source_example_is_stored(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from cxxman.core.document import Document, Paragraph
    from cxxman.storage.database import Database

    database_path = tmp_path / "docs.sqlite3"
    monkeypatch.setenv("CXXMAN_DB_PATH", str(database_path))
    with Database(database_path) as database:
        database.save_document(
            Document("id", "std::vector", "vector", "url", (Paragraph("sequence"),))
        )

    assert main(["vector", "--example"]) == 1
    assert "No source example is stored" in capsys.readouterr().err


def test_broken_pipe_exits_quietly(monkeypatch: pytest.MonkeyPatch) -> None:
    import sys

    class ClosedPipe:
        def write(self, _: str) -> int:
            raise BrokenPipeError()

        def flush(self) -> None:
            raise BrokenPipeError()

        def close(self) -> None:
            return None

    monkeypatch.setattr(sys, "stdout", ClosedPipe())
    assert main(["topics"]) == 0


def test_header_name_gets_header_match_in_not_found_guidance(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("CXXMAN_DB_PATH", str(tmp_path / "docs.sqlite3"))

    assert main(["algorithm"]) == 0
    assert "<algorithm>" in capsys.readouterr().out


def test_search_prints_header_and_source_purpose(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from cxxman.core.document import Document, Paragraph
    from cxxman.storage.database import Database

    database_path = tmp_path / "docs.sqlite3"
    monkeypatch.setenv("CXXMAN_DB_PATH", str(database_path))
    with Database(database_path) as database:
        database.save_document(
            Document(
                "cpp/unique",
                "std::unique",
                "unique",
                "url",
                (Paragraph("Algorithm details."),),
                header="<algorithm>",
                purpose="Removes consecutive duplicate elements.",
            )
        )

    assert main(["search", "duplicates"]) == 0
    output = capsys.readouterr().out
    assert "unique  <algorithm>  Removes consecutive duplicate elements." in output


def test_not_found_suggestions_include_catalog_aliases(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from dataclasses import replace

    from cxxman.core import catalog

    monkeypatch.setenv("CXXMAN_DB_PATH", str(tmp_path / "docs.sqlite3"))
    page = catalog.SOURCE_PAGES["vector"]
    monkeypatch.setitem(
        catalog.SOURCE_PAGES,
        "vector",
        replace(page, aliases=("dynamic_sequence",)),
    )

    assert main(["dynamic_sequenc"]) == 1
    assert "'vector' (cxxman vector)" in capsys.readouterr().err


def test_old_document_format_shows_targeted_update_hint(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from cxxman.core.document import Document, Paragraph
    from cxxman.storage.database import Database

    database_path = tmp_path / "docs.sqlite3"
    monkeypatch.setenv("CXXMAN_DB_PATH", str(database_path))
    with Database(database_path) as database:
        database.save_document(
            Document("cppref/vector", "std::vector", "vector", "url", (Paragraph("sequence"),))
        )
        with database._connection:
            database._connection.execute(
                "UPDATE documents SET format_version = 2 WHERE identifier = ?",
                ("cppref/vector",),
            )

    assert main(["vector"]) == 0
    assert "Run 'cxxman update vector' to refresh it." in capsys.readouterr().err


def test_read_commands_never_call_http(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    import httpx

    from cxxman.core.document import Document, Paragraph
    from cxxman.storage.database import Database

    database_path = tmp_path / "docs.sqlite3"
    monkeypatch.setenv("CXXMAN_DB_PATH", str(database_path))
    with Database(database_path) as database:
        database.save_document(
            Document("id", "std::vector", "vector", "url", (Paragraph("sequence"),))
        )

    def fail_if_called(*args, **kwargs):
        raise AssertionError("read commands must not make HTTP requests")

    monkeypatch.setattr(httpx.Client, "get", fail_if_called)
    monkeypatch.setattr("cxxman.parser.html_parser.parse_html", fail_if_called)
    assert main(["vector"]) == 0
    assert main(["search", "sequence"]) == 0
    assert main(["list"]) == 0


def test_update_command_rejects_unsupported_topic(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("CXXMAN_DB_PATH", str(tmp_path / "docs.sqlite3"))

    assert main(["update", "unknown_topic"]) == 1
    assert "unsupported topic" in capsys.readouterr().err


def test_update_command_fetches_requested_page(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    import httpx

    from cxxman.storage.database import Database

    database_path = tmp_path / "docs.sqlite3"
    monkeypatch.setenv("CXXMAN_DB_PATH", str(database_path))
    requested_urls: list[str] = []

    def fake_get(client: httpx.Client, url: str, **kwargs) -> httpx.Response:
        requested_urls.append(url)
        return httpx.Response(
            200,
            text="<html><body><h1>std::vector</h1><p>Sequence container.</p></body></html>",
            request=httpx.Request("GET", url),
        )

    monkeypatch.setattr(httpx.Client, "get", fake_get)
    assert main(["update", "vector"]) == 0
    captured = capsys.readouterr()
    assert "Update complete: 1 updated, 0 failed, 0 skipped." in captured.out
    assert "Open a page with: cxxman vector" in captured.out
    assert "Downloading vector from cppreference" in captured.err
    assert requested_urls == ["https://en.cppreference.com/cpp/container/vector"]

    with Database(database_path) as database:
        assert database.get_document("vector") is not None


def test_update_command_reports_partial_failures(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from cxxman.core.update import UpdateFailure, UpdateReport

    monkeypatch.setenv("CXXMAN_DB_PATH", str(tmp_path / "docs.sqlite3"))
    monkeypatch.setattr(
        "cxxman.core.update.update_documents",
        lambda *args, **kwargs: UpdateReport((), (UpdateFailure("sort", "HTTP 503"),)),
    )

    assert main(["update"]) == 1
    captured = capsys.readouterr()
    assert "0 updated, 1 failed, 0 skipped" in captured.out
    assert "Failed sort: HTTP 503" in captured.err


def test_corrupt_local_page_has_a_useful_error(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from cxxman.core.document import Document, Paragraph
    from cxxman.storage.database import Database

    database_path = tmp_path / "docs.sqlite3"
    monkeypatch.setenv("CXXMAN_DB_PATH", str(database_path))
    with Database(database_path) as database:
        database.save_document(
            Document("id", "std::vector", "vector", "url", (Paragraph("text"),))
        )
        with database._connection:
            database._connection.execute(
                "UPDATE documents SET content_json = ? WHERE identifier = ?",
                ("broken", "id"),
            )

    assert main(["vector"]) == 1
    assert "local page 'vector' is corrupt" in capsys.readouterr().err
