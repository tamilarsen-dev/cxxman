"""Tests for the initial command-line interface."""

import pytest

from cxxman import __version__
from cxxman.cli import main


def test_help(capsys: pytest.CaptureFixture[str]) -> None:
    with pytest.raises(SystemExit) as exc_info:
        main(["--help"])

    assert exc_info.value.code == 0
    assert "usage: cxxman" in capsys.readouterr().out


def test_version(capsys: pytest.CaptureFixture[str]) -> None:
    with pytest.raises(SystemExit) as exc_info:
        main(["--version"])

    assert exc_info.value.code == 0
    assert f"cxxman {__version__}" in capsys.readouterr().out


def test_topic_reports_not_available(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("CXXMAN_DB_PATH", str(tmp_path / "docs.sqlite3"))
    exit_code = main(["vector"])

    captured = capsys.readouterr()
    assert exit_code == 1
    assert "'vector' is not available locally" in captured.err
    assert "cxxman update" in captured.err


def test_search_list_and_topic_read_from_local_database(
    capsys: pytest.CaptureFixture[str], tmp_path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from cxxman.core.document import Document, Paragraph
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

    assert main(["list"]) == 0
    assert "vector  std::vector" in capsys.readouterr().out

    assert main(["search", "sequence"]) == 0
    assert "vector  std::vector" in capsys.readouterr().out

    assert main(["vector"]) == 0
    output = capsys.readouterr().out
    assert "std::vector" in output
    assert "A sequence container." in output


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
    assert "Updated 1 page(s): vector" in capsys.readouterr().out
    assert requested_urls == ["https://en.cppreference.com/cpp/container/vector"]

    with Database(database_path) as database:
        assert database.get_document("vector") is not None


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
