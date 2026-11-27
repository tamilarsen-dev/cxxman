// Task search (PLAN §8.3) over the real data: the tokenizer, the ranking and
// the six acceptance queries.
#include "search/search.hpp"
#include "search/tokenize.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace cxxman;

namespace {

using Words = std::vector<std::string>;

std::vector<std::string_view> names(std::string_view query) {
  const Catalog cat = Catalog::builtin();
  std::vector<std::string_view> out;
  for (const auto& h : search::search(cat, query)) out.push_back(search::hit_name(cat, h));
  return out;
}

// 1-based position of `name` in the results of `query`, or 0.
std::size_t position(std::string_view query, std::string_view name) {
  const auto n = names(query);
  const auto it = std::ranges::find(n, name);
  return it == n.end() ? 0 : static_cast<std::size_t>(it - n.begin()) + 1;
}

}  // namespace

TEST_CASE("tokenize: lowercase words without stop words") {
  CHECK(search::tokenize("Remove the Duplicates") == Words{"remove", "duplicates"});
  CHECK(search::tokenize("std::vector::push_back") == Words{"vector", "push", "back"});
  CHECK(search::tokenize("map-vs-unordered_map") == Words{"map", "vs", "unordered", "map"});
  CHECK(search::tokenize("how to sort a list?") == Words{"sort", "list"});
  CHECK(search::tokenize("  ").empty());
  CHECK(search::phrase("Remove  duplicates") == "remove duplicates");
}

TEST_CASE("search acceptance cases (PLAN 8.3)") {
  CHECK(position("remove duplicates", "unique") == 1);
  CHECK(position("dynamic array", "vector") == 1);
  CHECK(position("erase element", "vector::erase") >= 1);
  CHECK(position("erase element", "vector::erase") <= 3);
  CHECK(position("maybe value", "optional") == 1);
  CHECK(position("split string", "split-string") == 1);
  CHECK(position("hash map", "map-vs-unordered_map") >= 1);
  CHECK(position("hash map", "map-vs-unordered_map") <= 3);
}

TEST_CASE("search: matching rules") {
  const Catalog cat = Catalog::builtin();
  // Only stop words: nothing to look for.
  CHECK(search::search(cat, "the of a").empty());
  CHECK(search::search(cat, "zzzzqq").empty());
  // A prefix of three or more letters matches a word of a card.
  CHECK(position("dedup", "unique") == 1);
  // Shorter prefixes do not.
  CHECK(position("de", "unique") == 0);
  // At most ten results by default.
  CHECK(search::search(cat, "string").size() == search::max_hits);
  CHECK(search::search(cat, "string", 3).size() == 3);
}

TEST_CASE("search: scores follow PLAN 8.3 (doubled)") {
  const Catalog cat = Catalog::builtin();
  auto score_of = [&](std::string_view query, std::string_view name) {
    for (const auto& h : search::search(cat, query, 1000))
      if (search::hit_name(cat, h) == name) return h.score;
    return 0;
  };
  // unique: tag words "remove" (15) and "duplicates" (15), the whole tag
  // "remove duplicates" (25), every word matched (x1.5).
  CHECK(score_of("remove duplicates", "unique") == (15 + 15 + 25) * 2 * 3 / 2);
  // split-string: two id words (30 each), every word matched.
  CHECK(score_of("split string", "split-string") == (30 + 30) * 2 * 3 / 2);
  // An index name without a card: 10 per name word.
  CHECK(score_of("unordered", "unordered_map") == 10 * 2 * 3 / 2);
}

TEST_CASE("search: an index name in several headers is one result") {
  const Catalog cat = Catalog::builtin();
  const auto hits = search::search(cat, "erase element");
  const auto it = std::ranges::find_if(hits, [&](const search::Hit& h) {
    return h.target == Target::Index && search::hit_name(cat, h) == "erase";
  });
  REQUIRE(it != hits.end());
  CHECK(it->headers > 1);
  CHECK(std::ranges::count_if(hits, [&](const search::Hit& h) {
          return search::hit_name(cat, h) == "erase";
        }) == 1);
}

TEST_CASE("search: ties prefer cards, then shorter names") {
  const Catalog cat = Catalog::builtin();
  const auto hits = search::search(cat, "sort", 1000);
  REQUIRE(!hits.empty());
  CHECK(search::hit_name(cat, hits.front()) == "sort");
  CHECK(hits.front().target == Target::Entry);
  for (std::size_t i = 1; i < hits.size(); ++i) CHECK(hits[i - 1].score >= hits[i].score);
}
