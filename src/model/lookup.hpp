// Name resolution for `cxxman <name>` (PLAN §3.4).
#pragma once

#include "model/model.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cxxman {

// The tables lookup works on: the compiled-in data, or test fixtures. `keys`
// must be sorted like data::keys().
struct Catalog {
  std::span<const Entry> entries;
  std::span<const IndexName> index;
  std::span<const Key> keys;

  static Catalog builtin() noexcept;
};

// Trims, drops a leading std:: and accepts '.' for "::" (vector.push_back).
std::string normalize_name(std::string_view input);
std::string fold(std::string_view s);  // ASCII lowercase

struct Resolution {
  enum class What : std::uint8_t { Card, Index, Ambiguous, Missing };
  What what = What::Missing;
  std::int32_t entry = -1;                    // Card
  std::vector<std::int32_t> rows;             // Index: every row with the name
  std::vector<std::string_view> also;         // Card/Index: members of classes with the
                                              // same name; Ambiguous: the candidates
  std::vector<std::string_view> suggestions;  // Missing: did you mean
};

Resolution resolve(const Catalog& cat, std::string_view input);

// Up to `max` names (cards and index names) close to `query`, best first.
std::vector<std::string_view> suggest(const Catalog& cat, std::string_view query, std::size_t max = 5);

// The card whose id or alias is exactly `name` (after normalizing), or -1.
std::int32_t card_for(const Catalog& cat, std::string_view name);

}  // namespace cxxman
