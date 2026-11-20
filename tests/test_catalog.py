"""Tests for the default downloadable reference catalog."""

from cxxman.core.catalog import SOURCE_PAGES


def test_default_catalog_contains_fifty_distinct_cpp_pages() -> None:
    pages = tuple(SOURCE_PAGES.values())

    assert len(pages) == 50
    assert len({page.identifier for page in pages}) == 50
    assert len({page.url for page in pages}) == 50
    assert all(page.url.startswith("https://en.cppreference.com/cpp/") for page in pages)
    assert all(page.header and page.entity_kind != "unknown" for page in pages)


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
