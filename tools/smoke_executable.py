"""Run deterministic CLI and rendering checks against a frozen executable."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from cxxman.core.document import CodeBlock, Document, Paragraph
from cxxman.storage.database import Database


def main() -> int:
    executable = Path(sys.argv[1]).resolve()
    if not executable.is_file():
        raise SystemExit(f"Executable not found: {executable}")

    with tempfile.TemporaryDirectory(prefix="cxxman-smoke-") as temporary_directory:
        database_path = Path(temporary_directory) / "smoke.sqlite3"
        with Database(database_path) as database:
            database.save_document(
                Document(
                    "smoke/page",
                    "Smoke test page",
                    "smoke",
                    "https://example.invalid/smoke",
                    (
                        Paragraph("Standalone rendering works."),
                        CodeBlock("int main() {}", language="cpp"),
                    ),
                )
            )

        environment = os.environ.copy()
        environment["CXXMAN_DB_PATH"] = str(database_path)
        _run(executable, ["--help"], environment, expected="cxxman topics")
        _run(executable, ["--version"], environment, expected="cxxman ")
        _run(executable, [], environment, expected="Quick start:")
        _run(executable, ["topics"], environment, expected="vector::push_back")
        _run(executable, ["list"], environment, expected="Smoke test page")
        _run(executable, ["search", "Standalone"], environment, expected="cxxman smoke")
        _run(
            executable,
            ["smoke"],
            environment,
            expected="Standalone rendering works.",
        )

    print(f"Standalone executable smoke checks passed: {executable}")
    return 0


def _run(
    executable: Path,
    arguments: list[str],
    environment: dict[str, str],
    *,
    expected: str,
) -> None:
    result = subprocess.run(
        [str(executable), *arguments],
        check=False,
        capture_output=True,
        text=True,
        env=environment,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"{arguments!r} exited with {result.returncode}: {result.stderr.strip()}"
        )
    if expected not in result.stdout:
        raise RuntimeError(f"{arguments!r} output did not contain {expected!r}")


if __name__ == "__main__":
    raise SystemExit(main())
