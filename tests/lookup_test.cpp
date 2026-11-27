// Name resolution (PLAN §3.4) and the fuzzy scorer (PLAN §8.2).
#include "fixture_catalog.hpp"
#include "model/lookup.hpp"
#include "search/fuzzy.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <tuple>

using namespace cxxman;
using What = Resolution::What;

namespace {

std::string_view id_of(const Catalog& cat, const Resolution& r) {
  REQUIRE(r.what == What::Card);
  return cat.entries[static_cast<std::size_t>(r.entry)].id;
}

}  // namespace

TEST_CASE("normalize_name") {
  CHECK(normalize_name("  std::vector.push_back ") == "vector::push_back");
  CHECK(normalize_name("std::sort") == "sort");
  CHECK(normalize_name("ranges::sort") == "ranges::sort");
  CHECK(normalize_name("   ").empty());
}

TEST_CASE("fuzzy scorer: prefix beats one typo beats a subsequence") {
  using search::fuzzy_score;
  CHECK(fuzzy_score("sor", "sort") == 999);
  CHECK(fuzzy_score("SORT", "sort") == 1000);
  CHECK(fuzzy_score("srot", "sort") == 400);  // a transposition is one edit
  CHECK(fuzzy_score("emplce_back", "vector::emplace_back") == 400);  // the part after ::
  const int sub = fuzzy_score("vpb", "vector_push_back");
  CHECK(sub > 200);
  CHECK(sub < 400);
  CHECK(fuzzy_score("zzz", "sort") == 0);
  CHECK(search::edit_distance("kitten", "sitting", 5) == 3);
  CHECK(search::edit_distance("abcdef", "uvwxyz", 2) == 3);  // stops at bound + 1
}

TEST_CASE("cards by id, alias and any case") {
  const auto cat = Catalog::builtin();
  CHECK(id_of(cat, resolve(cat, "sort")) == "sort");
  CHECK(id_of(cat, resolve(cat, "std::ranges::sort")) == "sort");
  CHECK(id_of(cat, resolve(cat, "Vector.Erase")) == "vector::erase");
  CHECK(resolve(cat, "sort").also.empty());  // ranges::sort is the same card
}

TEST_CASE("an index name without a card") {
  const auto cat = Catalog::builtin();
  auto r = resolve(cat, "istringstream");
  REQUIRE(r.what == What::Index);
  REQUIRE(r.rows.size() == 1);
  CHECK(cat.index[static_cast<std::size_t>(r.rows[0])].header == "sstream");

  // std::erase lives in several headers; vector::erase is a member card.
  r = resolve(cat, "erase");
  REQUIRE(r.what == What::Index);
  CHECK(r.rows.size() == 7);
  CHECK(r.also == std::vector<std::string_view>{"vector::erase"});
}

TEST_CASE("a bare member name") {
  const auto cat = fixture::catalog();
  auto r = resolve(cat, "size");
  REQUIRE(r.what == What::Ambiguous);
  CHECK(r.also == std::vector<std::string_view>{"string::size", "vector::size"});
  CHECK(id_of(cat, resolve(cat, "vector::size")) == "vector::size");

  // Only one class has it: go straight there.
  const auto builtin = Catalog::builtin();
  r = resolve(builtin, "push_back");
  REQUIRE(r.what == What::Index);
  CHECK(builtin.index[static_cast<std::size_t>(r.rows[0])].name == "vector::push_back");
}

TEST_CASE("did you mean") {
  const auto cat = Catalog::builtin();
  auto r = resolve(cat, "srot");
  REQUIRE(r.what == What::Missing);
  CHECK(r.suggestions == std::vector<std::string_view>{"sort"});
  CHECK(resolve(cat, "vector::ersae").suggestions.front() == "vector::erase");
  CHECK(resolve(cat, "zzzz").suggestions.empty());
  CHECK(card_for(cat, "std::unique") >= 0);
  CHECK(card_for(cat, "remove") < 0);
}

TEST_CASE("known names: index rows plus cards missing from the index") {
  const auto cat = Catalog::builtin();
  const auto names = known_names(cat);
  CHECK(std::ranges::is_sorted(names, [](const NameInfo& a, const NameInfo& b) {
    return std::tie(a.name, a.header) < std::tie(b.name, b.header);
  }));
  auto find = [&](std::string_view name, std::string_view header) -> const NameInfo* {
    for (const auto& n : names)
      if (n.name == name && n.header == header) return &n;
    return nullptr;
  };
  // vector::erase has a card but no index row.
  const NameInfo* erase = find("vector::erase", "vector");
  REQUIRE(erase);
  CHECK(erase->entry >= 0);
  CHECK(erase->kind == Kind::Member);
  // The index does not know when sort arrived; its card does.
  const NameInfo* sort = find("sort", "algorithm");
  REQUIRE(sort);
  CHECK(sort->since == Std::Cpp98);
  CHECK(sort->kind == Kind::Algorithm);
  // An alias covers its index name.
  const NameInfo* ranges_sort = find("ranges::sort", "algorithm");
  REQUIRE(ranges_sort);
  CHECK(ranges_sort->entry == sort->entry);
}

TEST_CASE("normalize_header") {
  CHECK(normalize_header("<algorithm>") == "algorithm");
  CHECK(normalize_header(" vector ") == "vector");
  CHECK(normalize_header("<>") == "");
  CHECK(normalize_header("") == "");
}
