"""Reproducible local lookup, search, rendering, and startup measurements."""

from __future__ import annotations

import cProfile
import io
import json
import os
import os
import platform
import pstats
import sqlite3
import statistics
import subprocess
import sys
import tempfile
import time
import timeit
import tracemalloc
from pathlib import Path

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
from cxxman.storage.database import Database
from cxxman.ui.renderer import render_document

ROOT = Path(__file__).resolve().parents[1]
SMALL_CORPUS_SIZE = 3
LARGE_CORPUS_SIZE = 1_000
TIMEIT_REPEATS = 5
TIMEIT_NUMBER = 500


def make_document(index: int) -> Document:
    """Build a representative synthetic page with all common block types."""
    topic = f"symbol_{index:04d}"
    return Document(
        identifier=f"benchmark/{topic}",
        title=f"std::{topic}",
        canonical_topic=topic,
        source_url=f"https://example.test/{topic}",
        header="<benchmark>",
        blocks=(
            Heading("Overview", level=1),
            Paragraph("A sequence container with indexed lookup and search."),
            CodeBlock("template<class T> class example;", language="cpp"),
            ListBlock(("constant time lookup", "bounded search")),
            Table((("lookup", "logarithmic"),), headers=("Operation", "Complexity")),
            Reference("Related requirements", "cpp/container"),
        ),
    )


def median_milliseconds(operation, *, number: int = TIMEIT_NUMBER) -> float:
    samples = timeit.repeat(operation, number=number, repeat=TIMEIT_REPEATS)
    return statistics.median(samples) * 1_000 / number


def build_database(path: Path, count: int) -> Database:
    database = Database(path)
    database.save_documents([make_document(index) for index in range(count)])
    return database


def measure_startup(database_path: Path) -> dict[str, float]:
    command = [sys.executable, "-m", "cxxman", "search", "sequence"]
    environment = os.environ.copy()
    environment["CXXMAN_DB_PATH"] = str(database_path)
    samples: list[float] = []
    for _ in range(6):
        started = time.perf_counter()
        subprocess.run(
            command,
            cwd=ROOT,
            env=environment,
            capture_output=True,
            check=True,
        )
        samples.append((time.perf_counter() - started) * 1_000)
    return {"first_process_ms": samples[0], "warm_median_process_ms": statistics.median(samples[1:])}


def peak_page_load_bytes(database: Database) -> int:
    tracemalloc.start()
    baseline = tracemalloc.get_traced_memory()[0]
    document = database.get_document("symbol_0999")
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    if document is None:
        raise RuntimeError("benchmark page was not found")
    return peak - baseline


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="cxxman-benchmark-") as directory:
        temp_path = Path(directory)
        small_db = build_database(temp_path / "small.sqlite3", SMALL_CORPUS_SIZE)
        large_db = build_database(temp_path / "large.sqlite3", LARGE_CORPUS_SIZE)
        document = large_db.get_document("symbol_0999")
        if document is None:
            raise RuntimeError("benchmark page was not found")

        sink = io.StringIO()

        def render_once() -> None:
            sink.seek(0)
            sink.truncate(0)
            console = Console(file=sink, width=80, color_system=None, highlight=False)
            render_document(document, console=console)

        profile = cProfile.Profile()
        profile.runcall(lambda: [large_db.search("sequence container") for _ in range(100)])
        profile_output = io.StringIO()
        pstats.Stats(profile, stream=profile_output).sort_stats("cumulative").print_stats(6)

        small_metrics = {
            "pages": SMALL_CORPUS_SIZE,
            "database_bytes": (temp_path / "small.sqlite3").stat().st_size,
            "lookup_ms": median_milliseconds(lambda: small_db.get_document("symbol_0002")),
            "search_ms": median_milliseconds(lambda: small_db.search("sequence container")),
        }
        large_metrics = {
            "pages": LARGE_CORPUS_SIZE,
            "database_bytes": (temp_path / "large.sqlite3").stat().st_size,
            "lookup_ms": median_milliseconds(lambda: large_db.get_document("symbol_0999")),
            "search_ms": median_milliseconds(lambda: large_db.search("sequence container")),
            "page_load_python_peak_bytes": peak_page_load_bytes(large_db),
            "render_ms": median_milliseconds(render_once, number=100),
        }
        result = {
            "python": sys.version.split()[0],
            "platform": platform.platform(),
            "processor": platform.processor() or platform.machine(),
            "logical_cpus": os.cpu_count(),
            "sqlite": sqlite3.sqlite_version,
            "fts5": large_db.fts_enabled,
            "page_json_bytes": large_db._connection.execute(
                "SELECT SUM(length(content_json)) FROM documents"
            ).fetchone()[0],
            "startup": measure_startup(temp_path / "large.sqlite3"),
            "small_corpus": small_metrics,
            "large_corpus": large_metrics,
            "profile_top": profile_output.getvalue(),
        }
        print(json.dumps(result, indent=2))
        small_db.close()
        large_db.close()


if __name__ == "__main__":
    main()
