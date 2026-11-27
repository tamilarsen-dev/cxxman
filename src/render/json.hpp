// `--json` output (docs/JSON.md, schema 1). Hand-written: the runtime has no
// dependencies (PLAN §7.1). Every document ends with a newline.
#pragma once

#include "model/lookup.hpp"
#include "search/search.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace cxxman::render::json {

inline constexpr int schema = 1;

// `s` as a quoted JSON string. Control characters are escaped; bytes that
// are not valid UTF-8 become U+FFFD.
std::string quote(std::string_view s);

std::string card(const Entry& e, std::span<const std::string_view> also);
std::string index_only(const Catalog& cat, std::span<const std::int32_t> rows,
                       std::span<const std::string_view> also);
std::string ambiguous(std::string_view name, std::span<const std::string_view> candidates);
std::string missing(std::string_view name, std::span<const std::string_view> suggestions);
std::string search(const Catalog& cat, std::string_view query, std::span<const search::Hit> hits);
// `names`: the known names of header `name`; `h`: its header file, or null.
std::string header(std::string_view name, const Header* h, std::span<const NameInfo> names);

}  // namespace cxxman::render::json
