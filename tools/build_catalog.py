r"""Build the checked-in C++ standard-library catalog from cppreference.

Run explicitly with ``.venv\Scripts\python.exe tools\build_catalog.py``.
This maintainer tool performs network requests; normal cxxman commands only
read its checked-in JSON output.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from time import monotonic, sleep
from typing import Any

import httpx

from cxxman.core.catalog import _CATALOG_PATH
from cxxman.parser.catalog_parser import (
    HeaderIndexEntry,
    parse_header_index,
    parse_header_listing,
    parse_symbol_index_links,
    parse_symbol_index_pages,
)


BASE_URL = "https://en.cppreference.com/w/"
USER_AGENT = "cxxman catalog builder (https://github.com/tamilarsen-dev/cxxman)"
ATTRIBUTION = """Catalog metadata is derived from cppreference C++ library index pages.
Source: https://en.cppreference.com/w/cpp/header and
https://en.cppreference.com/w/cpp/symbol_index

cppreference content is available under CC-BY-SA 3.0 and the GNU Free
Documentation License. Each generated entry retains its source path.
"""


def fetch_page(client: httpx.Client, path: str, *, delay: float, last_request: list[float]) -> str:
    """Fetch one page politely, retrying transient source failures."""
    if last_request[0]:
        wait = delay - (monotonic() - last_request[0])
        if wait > 0:
            sleep(wait)
    normalized_path = path.removeprefix("/")
    if not normalized_path.startswith("cpp/"):
        normalized_path = "cpp/" + normalized_path
    url = BASE_URL + normalized_path
    last_error: httpx.HTTPError | None = None
    for attempt in range(3):
        last_request[0] = monotonic()
        try:
            response = client.get(url)
            response.raise_for_status()
            return response.text
        except httpx.HTTPError as error:
            last_error = error
            if attempt < 2:
                sleep(2**attempt)
    assert last_error is not None
    raise RuntimeError(f"could not fetch {url}: {last_error}") from last_error


def build_entries(
    existing: list[dict[str, Any]],
    header_pages: list[tuple[str, str, str]],
    symbol_links: list[tuple[str, str]],
) -> list[dict[str, Any]]:
    """Merge source rows while retaining the original default-update set."""
    defaults = {
        str(entry["topic"]).casefold(): bool(entry.get("default_update", False))
        for entry in existing
    }
    entries: list[dict[str, Any]] = []
    positions: dict[str, int] = {}

    for header, _source_path, html in header_pages:
        for parsed in parse_header_index(html, header=f"<{header}>"):
            parent_topic = next(
                (
                    str(entry["topic"])
                    for entry in entries
                    if entry["source_path"] == "/".join(parsed.source_path.split("/")[:-1])
                    and entry["entity_kind"] in {"class", "struct"}
                ),
                None,
            )
            row = _entry_from_header(parsed, header, parent_topic)
            key = str(row["topic"]).casefold()
            row["default_update"] = defaults.get(key, False)
            if key in positions:
                current = entries[positions[key]]
                current["aliases"] = _merge_names(current["aliases"], row["aliases"])
                if current.get("purpose") is None:
                    current["purpose"] = row["purpose"]
                continue
            positions[key] = len(entries)
            entries.append(row)

    for label, source_path in symbol_links:
        topic = _symbol_topic(label)
        key = topic.casefold()
        if not topic or key in positions:
            continue
        positions[key] = len(entries)
        entries.append(
            {
                "topic": topic,
                "aliases": [],
                "source_path": source_path,
                "header": None,
                "entity_kind": "unknown",
                "purpose": None,
                "parent": _parent_from_path(source_path, positions, entries),
                "group": "Symbol index",
                "default_update": defaults.get(key, False),
            }
        )

    for old in existing:
        key = str(old["topic"]).casefold()
        if key in positions:
            current = entries[positions[key]]
            current["default_update"] = bool(old.get("default_update", False))
            current["aliases"] = _merge_names(current["aliases"], old.get("aliases", []))
            if current.get("entity_kind") == "unknown":
                current["entity_kind"] = old.get("entity_kind", "unknown")
            if current.get("header") is None:
                current["header"] = old.get("header")
            if current.get("purpose") is None:
                current["purpose"] = old.get("purpose")
            continue
        retained = dict(old)
        retained["default_update"] = bool(old.get("default_update", False))
        positions[key] = len(entries)
        entries.append(retained)

    _assign_parents(entries)
    return entries


def _entry_from_header(
    parsed: HeaderIndexEntry, header: str, parent_topic: str | None
) -> dict[str, Any]:
    return {
        "topic": f"{parent_topic}::{parsed.topic}" if parent_topic else parsed.topic,
        "aliases": list(parsed.aliases),
        "source_path": parsed.source_path,
        "header": f"<{header}>",
        "entity_kind": parsed.entity_kind,
        "purpose": parsed.purpose,
        "parent": parent_topic,
        "group": parsed.group,
        "default_update": False,
    }


def _symbol_topic(label: str) -> str:
    normalized = " ".join(label.replace("std::", "").split())
    if "(" in normalized and not normalized.startswith("operator"):
        normalized = normalized.split("(", 1)[0].strip()
    if "," in normalized:
        normalized = normalized.split(",", 1)[0].strip()
    return normalized


def _merge_names(left: list[str], right: list[str]) -> list[str]:
    result = list(left)
    seen = {name.casefold() for name in result}
    for name in right:
        if name.casefold() not in seen:
            result.append(name)
            seen.add(name.casefold())
    return result


def _parent_from_path(
    source_path: str, positions: dict[str, int], entries: list[dict[str, Any]]
) -> str | None:
    parts = source_path.split("/")
    if len(parts) < 3:
        return None
    parent_path = "/".join(parts[:-1])
    for entry in entries:
        if entry["source_path"] == parent_path and entry["entity_kind"] in {"class", "struct"}:
            return str(entry["topic"])
    return None


def _assign_parents(entries: list[dict[str, Any]]) -> None:
    path_to_parent = {
        entry["source_path"]: entry["topic"]
        for entry in entries
        if entry["entity_kind"] in {"class", "struct"}
    }
    for entry in entries:
        parts = str(entry["source_path"]).split("/")
        if len(parts) < 3:
            entry["parent"] = None
            continue
        entry["parent"] = path_to_parent.get("/".join(parts[:-1]))


def _read_existing(path: Path) -> list[dict[str, Any]]:
    if not path.exists():
        return []
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("format_version") != 1 or not isinstance(data.get("entries"), list):
        raise ValueError(f"unsupported existing catalog format: {path}")
    return data["entries"]


def _write_catalog(path: Path, entries: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = {"format_version": 1, "entries": entries}
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    temporary.replace(path)
    attribution = path.with_name("README.txt")
    attribution.write_text(ATTRIBUTION, encoding="utf-8")


def _fetch_catalog(client: httpx.Client, *, delay: float) -> tuple[list[tuple[str, str, str]], list[tuple[str, str]]]:
    last_request = [0.0]
    header_index = fetch_page(client, "cpp/header", delay=delay, last_request=last_request)
    symbol_index = fetch_page(client, "cpp/symbol_index", delay=delay, last_request=last_request)
    header_paths = parse_header_listing(header_index)
    header_pages: list[tuple[str, str, str]] = []
    for name, path in header_paths:
        page_html = fetch_page(client, path, delay=delay, last_request=last_request)
        header_pages.append((name, path, page_html))

    symbol_links: list[tuple[str, str]] = []
    index_paths = parse_symbol_index_pages(symbol_index)
    for path in index_paths:
        page_html = fetch_page(client, path, delay=delay, last_request=last_request)
        symbol_links.extend(parse_symbol_index_links(page_html))
    return header_pages, symbol_links


def _read_fixture_pages(directory: Path) -> list[tuple[str, str, str]]:
    pages: list[tuple[str, str, str]] = []
    for path in sorted(directory.glob("header_*.html")):
        name = path.stem.removeprefix("header_")
        pages.append((name, f"header/{name}", path.read_text(encoding="utf-8")))
    if not pages:
        raise ValueError(f"no header_*.html fixtures found in {directory}")
    return pages


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=_CATALOG_PATH)
    parser.add_argument("--delay", type=float, default=1.0, help="minimum seconds between requests")
    parser.add_argument(
        "--fixture-dir",
        type=Path,
        help="build offline from saved header_*.html pages (for parser verification)",
    )
    args = parser.parse_args()
    if args.delay < 0:
        parser.error("--delay must be non-negative")

    existing = [
        entry
        for entry in _read_existing(args.output)
        if bool(entry.get("default_update", False))
    ]
    if args.fixture_dir:
        header_pages = _read_fixture_pages(args.fixture_dir)
        symbol_links: list[tuple[str, str]] = []
    else:
        with httpx.Client(
            timeout=30.0,
            follow_redirects=True,
            headers={"User-Agent": USER_AGENT},
        ) as client:
            header_pages, symbol_links = _fetch_catalog(client, delay=args.delay)

    entries = build_entries(existing, header_pages, symbol_links)
    _write_catalog(args.output, entries)
    print(f"wrote {len(entries)} catalog entries to {args.output}")
    print(f"{sum(bool(entry['default_update']) for entry in entries)} remain in the default update set")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
