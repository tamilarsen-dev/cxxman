// The finder's candidates and ranking (PLAN §4.6), and its benchmark (§10).
#include "tui/finder_model.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace cxxman;
using tui::Item;
using K = Item::Kind;

namespace {

const tui::FinderModel& model() {
  static const Catalog cat = Catalog::builtin();
  static const tui::FinderModel m(cat);
  return m;
}

std::vector<std::string_view> names(const std::vector<tui::Match>& ms, std::size_t n = 5) {
  std::vector<std::string_view> out;
  for (std::size_t i = 0; i < ms.size() && i < n; ++i) out.push_back(model().item(ms[i].item).name);
  return out;
}

}  // namespace

TEST_CASE("finder items: every card, recipe, compare and card-less name once") {
  const auto& m = model();
  const Catalog cat = Catalog::builtin();
  std::set<std::string_view> seen;
  std::size_t cards = 0, names_ = 0;
  for (std::uint32_t i = 0; i < m.size(); ++i) {
    const Item& it = m.item(i);
    CHECK(it.kind != K::Header);
    if (it.kind == K::Name) {
      ++names_;
      CHECK(seen.insert(it.name).second);  // one line per name, however many headers
      CHECK(cat.index[static_cast<std::size_t>(it.index)].entry < 0);
    }
    if (it.kind == K::Card) ++cards;
  }
  CHECK(cards == cat.entries.size());
  CHECK(names_ > 2000);
  // The empty query lists everything but headers, cards first.
  const auto all = m.find("");
  CHECK(all.size() == m.size());
  CHECK(m.item(all.front().item).kind == K::Card);
}

TEST_CASE("finder ranks by the fuzzy scorer, best key per item") {
  const auto& m = model();
  const auto sort = m.find("sort");
  REQUIRE(!sort.empty());
  CHECK(m.item(sort[0].item).name == "sort");
  CHECK(m.item(sort[0].item).kind == K::Card);
  CHECK(m.item(sort[0].item).header == "algorithm");
  CHECK(names(m.find("std::sort"), 1) == names(sort, 1));
  CHECK(names(m.find("  SORT "), 3) == names(sort, 3));
  CHECK(names(m.find("srot"), 1) == std::vector<std::string_view>{"sort"});  // a typo
  // An alias finds its card: ranges::sort is an alias of sort.
  CHECK(names(m.find("ranges::sort"), 1) == std::vector<std::string_view>{"sort"});
  // Index names show up after cards with the same score.
  const auto vec = m.find("vector");
  CHECK(m.item(vec[0].item).name == "vector");
  // No headers outside '<'.
  for (const auto& x : m.find("algorithm")) CHECK(m.item(x.item).kind != K::Header);
  CHECK(m.find("zzzzqqq").empty());
}

TEST_CASE("finder prefixes: < headers, # recipes, = compares") {
  const auto& m = model();
  const auto h = m.find("<alg");
  REQUIRE(!h.empty());
  CHECK(m.item(h[0].item).name == "algorithm");
  for (const auto& x : h) CHECK(m.item(x.item).kind == K::Header);
  CHECK(names(m.find("<algorithm>"), 1) == std::vector<std::string_view>{"algorithm"});
  CHECK(m.find("<").size() > 50);  // every header

  const auto r = m.find("#");
  REQUIRE(r.size() == 1);
  CHECK(m.item(r[0].item).kind == K::Recipe);
  CHECK(names(m.find("#split"), 1) == std::vector<std::string_view>{"split-string"});
  CHECK(m.find("#sort").empty());

  const auto c = m.find("=map");
  REQUIRE(c.size() == 1);
  CHECK(m.item(c[0].item).name == "map-vs-unordered_map");
}

TEST_CASE("finder uses task search when the query has a space") {
  const auto& m = model();
  const auto hits = m.find("remove duplicates");
  REQUIRE(!hits.empty());
  CHECK(m.item(hits[0].item).name == "unique");
  CHECK(names(m.find("split string"), 1) == std::vector<std::string_view>{"split-string"});
}

// PLAN §10: scoring all keys must take at most 2 ms. Only an optimized build
// is held to it; a debug build reports the time.
TEST_CASE("finder benchmark: one query over every key") {
  const auto& m = model();
  for (std::string_view q : {"s", "sort", "emplce_back", "unordered_map", "remove duplicates"}) {
    std::vector<double> ms;
    for (int i = 0; i < 21; ++i) {
      const auto t0 = std::chrono::steady_clock::now();
      const auto r = m.find(q);
      const auto t1 = std::chrono::steady_clock::now();
      CHECK(r.size() < m.size() + 1);
      ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::ranges::sort(ms);
    const double median = ms[ms.size() / 2];
    MESSAGE("finder '", std::string(q), "': ", median, " ms (median of 21)");
#ifdef NDEBUG
    CHECK(median <= 2.0);
#endif
  }
}
