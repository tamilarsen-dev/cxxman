// Layouts for `cxxman <name>` (PLAN §4.3-4.5).
#pragma once

#include "model/lookup.hpp"
#include "render/layout.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace cxxman::render {

struct Options {
  std::size_t width = 80;  // 60..100
  bool ascii = false;
  Std std = Std::Unknown;  // --std: mark what is newer; Unknown: mark nothing
  bool strict = false;     // --strict: hide what is newer instead
  bool support = false;    // --support: always show the support line
};

// The standard a badge names: Cpp20 for "constexpr C++20"; Unknown if none.
Std std_of_badge(std::string_view badge);

// The full cppreference URL of a url path from the data.
std::string reference_url(std::string_view path);
// The cppreference page of a header: "algorithm" -> .../header/algorithm.
std::string header_url(std::string_view header);
// The page of the card for `name` (id or alias), or "" when it has none.
std::string card_url(const Catalog& cat, std::string_view name);

// Function, algorithm, member and class cards. `also` lists members of other
// classes with the same name.
Block card(const Catalog& cat, const Entry& e, std::span<const std::string_view> also,
           const Options& o);

// Card parts (PLAN §3.2): the title with every example, code never wrapped
// (-e); the title with the signatures (--sig); one line (-b).
Block examples_only(const Entry& e, const Options& o);
Block signatures_only(const Entry& e, const Options& o);
Block brief(const Entry& e, const Options& o);

// A known name without a card: header(s) and the cppreference URL(s).
Block index_only(const Catalog& cat, std::span<const std::int32_t> rows,
                 std::span<const std::string_view> also, const Options& o);

// For stderr.
Block not_found(std::string_view input, std::span<const std::string_view> suggestions,
                const Options& o);
Block ambiguous(std::string_view input, std::span<const std::string_view> candidates,
                const Options& o);

}  // namespace cxxman::render
