// Layouts for `cxxman <name>` (PLAN §4.3-4.5).
#pragma once

#include "model/lookup.hpp"
#include "render/layout.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace cxxman::render {

struct Options {
  std::size_t width = 80;  // 60..100
  bool ascii = false;
};

// Function, algorithm, member and class cards. `also` lists members of other
// classes with the same name.
Block card(const Catalog& cat, const Entry& e, std::span<const std::string_view> also,
           const Options& o);

// A known name without a card: header(s) and the cppreference URL(s).
Block index_only(const Catalog& cat, std::span<const std::int32_t> rows,
                 std::span<const std::string_view> also, const Options& o);

// For stderr.
Block not_found(std::string_view input, std::span<const std::string_view> suggestions,
                const Options& o);
Block ambiguous(std::string_view input, std::span<const std::string_view> candidates,
                const Options& o);

}  // namespace cxxman::render
