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
  std::span<const Recipe> recipes{};
  std::span<const Compare> compares{};
  std::span<const Header> headers{};
  std::span<const Term> terms{};  // sorted by term

  static Catalog builtin() noexcept;
};

// One known name in one header: an index row, or a card missing from the
// index. Kind and since come from the card when the index does not know them
// and the card is about the same header.
struct NameInfo {
  std::string_view name{}, header{};
  Kind kind = Kind::Unknown;
  Std since = Std::Unknown;
  std::int32_t entry = -1;  // the covering card, or -1
};

// Every (name, header) pair once, sorted by name, then header.
std::vector<NameInfo> known_names(const Catalog& cat);

// "algorithm" for "<algorithm>", " algorithm " or "algorithm".
std::string normalize_header(std::string_view input);

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
