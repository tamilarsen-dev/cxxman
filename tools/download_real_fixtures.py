"""Download and trim the real cppreference pages used by offline tests.

Run explicitly with ``.venv\\Scripts\\python.exe tools\\download_real_fixtures.py``.
This tool is never imported by cxxman or used by the page-view/search path.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from time import sleep
from urllib.request import Request, urlopen

from bs4 import BeautifulSoup


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = REPOSITORY_ROOT / "tests" / "fixtures" / "real"
BASE_URL = "https://en.cppreference.com"
PAGES = {
    "vector": "/w/cpp/container/vector",
    "sort": "/w/cpp/algorithm/sort",
    "all_any_none_of": "/w/cpp/algorithm/all_any_none_of",
    "vector_push_back": "/w/cpp/container/vector/push_back",
    "optional": "/w/cpp/utility/optional",
    "is_same": "/w/cpp/types/is_same",
    "same_as": "/w/cpp/concepts/same_as",
    "cout": "/w/cpp/io/cout",
    "header_algorithm": "/w/cpp/header/algorithm",
}
ATTRIBUTION = """These fixtures are trimmed copies of cppreference content areas.
Source pages:
  https://en.cppreference.com/w/cpp/container/vector
  https://en.cppreference.com/w/cpp/algorithm/sort
  https://en.cppreference.com/w/cpp/algorithm/all_any_none_of
  https://en.cppreference.com/w/cpp/container/vector/push_back
  https://en.cppreference.com/w/cpp/utility/optional
  https://en.cppreference.com/w/cpp/types/is_same
  https://en.cppreference.com/w/cpp/concepts/same_as
  https://en.cppreference.com/w/cpp/io/cout
  https://en.cppreference.com/w/cpp/header/algorithm

Source text and markup are by cppreference contributors and are licensed
under CC-BY-SA 3.0 and the GNU Free Documentation License (GFDL). These
copies retain attribution and the source licenses; see cppreference's
licensing information at https://en.cppreference.com/w/Cppreference:Copyright.
"""
CHROME_SELECTORS = (
    ".t-navbar",
    ".toc",
    ".mw-editsection",
    ".catlinks",
    ".printfooter",
    "script",
    "style",
    "footer",
)


def fetch_trimmed_page(topic: str, path: str) -> str:
    """Fetch one source page and retain its parser-output content markup."""
    url = BASE_URL + path
    request = Request(
        url,
        headers={"User-Agent": "cxxman-real-fixture-tests/1.0"},
    )
    with urlopen(request, timeout=30) as response:
        source = response.read().decode(response.headers.get_content_charset() or "utf-8")

    soup = BeautifulSoup(source, "html.parser")
    content = soup.select_one("#mw-content-text .mw-parser-output")
    if content is None:
        raise RuntimeError(f"Could not find cppreference content area for {url}")

    heading = soup.select_one("#firstHeading") or content.find("h1")
    title = " ".join(heading.get_text("", strip=True).split()) if heading else topic
    trimmed = BeautifulSoup("<html><head></head><body></body></html>", "html.parser")
    title_tag = trimmed.new_tag("title")
    title_tag.string = title
    assert trimmed.head is not None
    trimmed.head.append(title_tag)
    for selector in CHROME_SELECTORS:
        for node in content.select(selector):
            node.decompose()
    body = trimmed.body
    assert body is not None
    wrapper = trimmed.new_tag("div", id="mw-content-text")
    body.append(wrapper)
    wrapper.append(content)
    return "<!doctype html>\n" + str(trimmed)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=OUTPUT_DIR,
        help="fixture output directory (default: tests/fixtures/real)",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="replace existing fixture files after explicitly reviewing them",
    )
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    existing = [args.output_dir / f"{topic}.html" for topic in PAGES]
    if not args.overwrite and any(path.exists() for path in existing):
        parser.error("fixture files already exist; pass --overwrite to replace them")

    for index, (topic, path) in enumerate(PAGES.items()):
        fixture = fetch_trimmed_page(topic, path)
        (args.output_dir / f"{topic}.html").write_text(fixture, encoding="utf-8")
        print(f"saved {topic}.html")
        if index + 1 < len(PAGES):
            sleep(1)

    (args.output_dir / "README.txt").write_text(ATTRIBUTION, encoding="utf-8")
    print(f"attribution saved to {args.output_dir / 'README.txt'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
