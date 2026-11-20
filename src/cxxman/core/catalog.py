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
    entity_kind: str


# (topic, cppreference path after /cpp/, header, semantic entity kind)
_PAGE_DATA = (
    ("vector", "container/vector", "<vector>", "class"),
    ("string", "string/basic_string", "<string>", "class"),
    ("sort", "algorithm/sort", "<algorithm>", "algorithm"),
    ("unique", "algorithm/unique", "<algorithm>", "algorithm"),
    ("optional", "utility/optional", "<optional>", "class"),
    ("vector::push_back", "container/vector/push_back", "<vector>", "member function"),
    ("array", "container/array", "<array>", "class"),
    ("deque", "container/deque", "<deque>", "class"),
    ("forward_list", "container/forward_list", "<forward_list>", "class"),
    ("list", "container/list", "<list>", "class"),
    ("map", "container/map", "<map>", "class"),
    ("multimap", "container/multimap", "<map>", "class"),
    ("multiset", "container/multiset", "<set>", "class"),
    ("set", "container/set", "<set>", "class"),
    ("unordered_map", "container/unordered_map", "<unordered_map>", "class"),
    ("unordered_multimap", "container/unordered_multimap", "<unordered_map>", "class"),
    ("unordered_multiset", "container/unordered_multiset", "<unordered_set>", "class"),
    ("unordered_set", "container/unordered_set", "<unordered_set>", "class"),
    ("stack", "container/stack", "<stack>", "class"),
    ("queue", "container/queue", "<queue>", "class"),
    ("priority_queue", "container/priority_queue", "<queue>", "class"),
    ("span", "container/span", "<span>", "class"),
    ("string_view", "string/basic_string_view", "<string_view>", "class"),
    ("bitset", "utility/bitset", "<bitset>", "class"),
    ("all_of", "algorithm/all_of", "<algorithm>", "algorithm"),
    ("adjacent_find", "algorithm/adjacent_find", "<algorithm>", "algorithm"),
    ("equal", "algorithm/equal", "<algorithm>", "algorithm"),
    ("binary_search", "algorithm/binary_search", "<algorithm>", "algorithm"),
    ("copy", "algorithm/copy", "<algorithm>", "algorithm"),
    ("count", "algorithm/count", "<algorithm>", "algorithm"),
    ("find", "algorithm/find", "<algorithm>", "algorithm"),
    ("for_each", "algorithm/for_each", "<algorithm>", "algorithm"),
    ("lower_bound", "algorithm/lower_bound", "<algorithm>", "algorithm"),
    ("max_element", "algorithm/max_element", "<algorithm>", "algorithm"),
    ("min_element", "algorithm/min_element", "<algorithm>", "algorithm"),
    ("remove", "algorithm/remove", "<algorithm>", "algorithm"),
    ("replace", "algorithm/replace", "<algorithm>", "algorithm"),
    ("reverse", "algorithm/reverse", "<algorithm>", "algorithm"),
    ("transform", "algorithm/transform", "<algorithm>", "algorithm"),
    ("fill", "algorithm/fill", "<algorithm>", "algorithm"),
    ("generate", "algorithm/generate", "<algorithm>", "algorithm"),
    ("accumulate", "algorithm/accumulate", "<numeric>", "algorithm"),
    ("merge", "algorithm/merge", "<algorithm>", "algorithm"),
    ("any", "utility/any", "<any>", "class"),
    ("variant", "utility/variant", "<variant>", "class"),
    ("pair", "utility/pair", "<utility>", "class"),
    ("tuple", "utility/tuple", "<tuple>", "class"),
    ("expected", "utility/expected", "<expected>", "class"),
    ("unique_ptr", "memory/unique_ptr", "<memory>", "class"),
    ("shared_ptr", "memory/shared_ptr", "<memory>", "class"),
)


SOURCE_PAGES = {
    topic: SourcePage(
        topic=topic,
        identifier=f"cppref/{path}",
        url=f"https://en.cppreference.com/cpp/{path}",
        header=header,
        entity_kind=entity_kind,
    )
    for topic, path, header, entity_kind in _PAGE_DATA
}
