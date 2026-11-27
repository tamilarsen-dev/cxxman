// Task search (PLAN §8.3): "remove duplicates" -> unique.
#pragma once

#include "model/lookup.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace cxxman::search {

// Per document, each query word adds its best match: an id/alias word 30, a
// tag word 15, a summary/title word 8, a prefix (3+ chars) of any of these 5.
// A query equal to a whole tag adds 25. Index names without a card score 10
// per name word. Every word matched: times 1.5. Scores here are doubled so
// the factor stays an integer.
struct Hit {
  Target target = Target::Entry;
  std::int32_t index = -1;  // Index: the first row with the name
  int score = 0;
  std::size_t headers = 1;  // Index: how many headers have the name
};

inline constexpr std::size_t max_hits = 10;

// Best first; ties prefer cards, then recipes and compares, then index names,
// then the shorter name, then alphabetical order.
std::vector<Hit> search(const Catalog& cat, std::string_view query, std::size_t max = max_hits);

// The name a hit is shown and opened by: card id, recipe id, compare id or
// index name.
std::string_view hit_name(const Catalog& cat, const Hit& h);

}  // namespace cxxman::search
