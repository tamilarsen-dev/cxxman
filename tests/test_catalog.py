"""Tests for the default downloadable reference catalog."""

from cxxman.core.catalog import SOURCE_PAGES, default_update_pages, pages_for_header


def test_generated_catalog_contains_distinct_cpp_pages_and_preserves_the_default_set() -> None:
    pages = tuple(SOURCE_PAGES.values())

    assert len(pages) >= 50
    assert len({page.topic.casefold() for page in pages}) == len(pages)
    assert all(page.url.startswith("https://en.cppreference.com/cpp/") for page in pages)
    defaults = default_update_pages()
    assert len(defaults) == 50
    assert all(page.default_update for page in defaults)
    assert {"vector", "sort", "optional", "shared_ptr"} <= {page.topic for page in defaults}


def test_catalog_covers_major_library_areas() -> None:
    expected = {
        "vector": ("<vector>", "class"),
        "sort": ("<algorithm>", "algorithm"),
        "optional": ("<optional>", "class"),
        "shared_ptr": ("<memory>", "class"),
    }

    assert {"vector", "sort", "optional", "shared_ptr"} <= SOURCE_PAGES.keys()
    for topic, (header, entity_kind) in expected.items():
        assert (SOURCE_PAGES[topic].header, SOURCE_PAGES[topic].entity_kind) == (
            header,
            entity_kind,
        )


def test_catalog_uses_the_correct_numeric_header_and_cppreference_route() -> None:
    page = SOURCE_PAGES["accumulate"]

    assert page.header == "<numeric>"
    assert page.url == "https://en.cppreference.com/cpp/algorithm/accumulate"


def test_header_lookup_accepts_bracketed_and_plain_names() -> None:
    assert pages_for_header("vector") == pages_for_header("<vector>")
    assert any(page.topic == "vector" for page in pages_for_header("<vector>"))


def test_catalog_preserves_multi_name_rows_and_class_member_ownership() -> None:
    assert SOURCE_PAGES["all_of"].aliases == ("any_of", "none_of")
    member = SOURCE_PAGES["vector::push_back"]
    assert member.parent == "vector"
    assert member.source_path == "container/vector/push_back"
