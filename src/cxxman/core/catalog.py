"""Curated reference pages that cxxman can download."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class SourcePage:
    """Source metadata for one supported reference page."""

    topic: str
    identifier: str
    url: str
    header: str | None


SOURCE_PAGES = {
    "vector": SourcePage(
        "vector", "cppref/container/vector", "https://en.cppreference.com/cpp/container/vector", "<vector>"
    ),
    "string": SourcePage(
        "string", "cppref/string/basic_string", "https://en.cppreference.com/cpp/string/basic_string", "<string>"
    ),
    "sort": SourcePage(
        "sort", "cppref/algorithm/sort", "https://en.cppreference.com/cpp/algorithm/sort", "<algorithm>"
    ),
    "vector::push_back": SourcePage(
        "vector::push_back",
        "cppref/container/vector/push_back",
        "https://en.cppreference.com/cpp/container/vector/push_back",
        "<vector>",
    ),
}
