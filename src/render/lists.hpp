// Discovery layouts (PLAN §4.5): search results, the header index, --list,
// --new and --coverage.
#pragma once

#include "model/lookup.hpp"
#include "render/card.hpp"
#include "search/search.hpp"

#include <span>
#include <string_view>

namespace cxxman::render {

// One line per hit: kind marker, name, header (or recipe/compare), summary.
Block search_results(const Catalog& cat, std::span<const search::Hit> hits, const Options& o);

// `-H`: the header's groups, then its other names. `names` are the known
// names of that header; `h` is its header file, or null.
Block header_index(const Catalog& cat, std::string_view header, const Header* h,
                   std::span<const NameInfo> names, const Options& o);

// `--list`: one name per line with header, kind and since.
Block name_list(const Catalog& cat, std::span<const NameInfo> names, const Options& o);

// `--new`: the names added in `s`, grouped by header.
Block whats_new(const Catalog& cat, std::span<const NameInfo> names, Std s, const Options& o);

// `--coverage`: names with cards per header, most uncovered first. `names` is
// every known name.
Block coverage(std::span<const NameInfo> names, const Options& o);

// `--coverage --missing`: the names of one header without a card.
Block missing(std::string_view header, std::span<const NameInfo> names, const Options& o);

}  // namespace cxxman::render
