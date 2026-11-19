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


def test_topic_reports_not_available(capsys: pytest.CaptureFixture[str]) -> None:
    exit_code = main(["vector"])

    captured = capsys.readouterr()
    assert exit_code == 1
    assert "'vector' is not available locally" in captured.err
    assert "cxxman update" in captured.err
