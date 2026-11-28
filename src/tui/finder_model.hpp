// What the finder can show and how a query ranks it (PLAN §4.6), apart from
// any terminal: built once from the catalog, queried on every keystroke.
#pragma once

#include "model/lookup.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace cxxman::tui {

struct Item {
  enum class Kind : std::uint8_t { Card, Name, Recipe, Compare, Header };
  Kind kind = Kind::Card;
  std::int32_t index = -1;   // into entries, index (its first row), recipes or compares; -1 for Header
  std::string_view name{};   // what the list shows and what opens it
  std::string_view header{};  // Card, Name: its header; else empty
};

struct Match {
  std::uint32_t item = 0;
  int score = 0;
};

class FinderModel {
 public:
  explicit FinderModel(const Catalog& cat);

  // Ranked matches, best first. Prefixes: '<' headers, '#' recipes only,
  // '=' compares only. A query with a space inside uses task search
  // (§8.3); otherwise the fuzzy scorer (§8.2) over every name and alias.
  // An empty query lists cards, recipes, compares, then names, A to Z.
  std::vector<Match> find(std::string_view query) const;

  const Item& item(std::uint32_t i) const { return items_[i]; }
  std::size_t size() const { return headers_from_; }  // every item but headers

 private:
  struct KeyRef {
    std::string_view key;
    std::uint32_t item;
  };
  std::vector<Match> rank(std::string_view q, Item::Kind only, bool any) const;
  std::vector<Match> tasks(std::string_view q) const;

  const Catalog* cat_;
  std::vector<Item> items_;  // cards, recipes, compares, names, then headers
  std::size_t headers_from_ = 0;
  std::vector<KeyRef> keys_;   // every key of every item, header names last
  std::vector<KeyRef> names_;  // index names without a card, sorted, -> item
  std::vector<std::int32_t> entry_item_, recipe_item_, compare_item_;  // index -> item
};

}  // namespace cxxman::tui
