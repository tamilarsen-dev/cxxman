// The generated tables in data.cpp, built from the real data/.
#include "model/model.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <tuple>
#include <vector>

using namespace cxxman;

namespace {

std::vector<const Key*> keys_named(std::string_view key) {
  std::vector<const Key*> out;
  for (const auto& k : data::keys())
    if (k.key == key) out.push_back(&k);
  return out;
}

}  // namespace

TEST_CASE("tables are sorted by id") {
  CHECK(std::ranges::is_sorted(data::entries(), {}, &Entry::id));
  CHECK(std::ranges::is_sorted(data::recipes(), {}, &Recipe::id));
  CHECK(std::ranges::is_sorted(data::compares(), {}, &Compare::id));
  CHECK(std::ranges::is_sorted(data::headers(), {}, &Header::header));
  CHECK(std::ranges::is_sorted(data::index_names(), {}, &IndexName::name));
  CHECK(std::ranges::is_sorted(data::keys(), [](const Key& a, const Key& b) {
    return std::tie(a.folded, a.key, a.target, a.index) < std::tie(b.folded, b.key, b.target, b.index);
  }));
}

TEST_CASE("keys point at their targets") {
  for (const auto& k : data::keys()) {
    REQUIRE(k.index >= 0);
    switch (k.target) {
      case Target::Entry: CHECK(static_cast<std::size_t>(k.index) < data::entries().size()); break;
      case Target::Recipe: CHECK(static_cast<std::size_t>(k.index) < data::recipes().size()); break;
      case Target::Compare: CHECK(static_cast<std::size_t>(k.index) < data::compares().size()); break;
      case Target::Index: CHECK(static_cast<std::size_t>(k.index) < data::index_names().size()); break;
    }
    CHECK_FALSE(k.key.starts_with("std::"));
  }
}

TEST_CASE("a card is reachable by id and alias, and covers its index name") {
  auto by_alias = keys_named("ranges::sort");
  auto it = std::ranges::find(by_alias, Target::Entry, &Key::target);
  REQUIRE(it != by_alias.end());
  const Entry& sort = data::entries()[static_cast<std::size_t>((*it)->index)];
  CHECK(sort.id == "sort");
  CHECK(sort.header == "algorithm");
  CHECK(sort.kind == Kind::Algorithm);
  CHECK_FALSE(sort.examples.empty());

  auto names = data::index_names();
  auto row = std::ranges::find(names, std::string_view("sort"), &IndexName::name);
  REQUIRE(row != names.end());
  CHECK(row->entry >= 0);
  CHECK(data::entries()[static_cast<std::size_t>(row->entry)].id == "sort");
}

TEST_CASE("search postings (PLAN 7.3)") {
  const auto terms = data::terms();
  REQUIRE_FALSE(terms.empty());
  CHECK(std::ranges::is_sorted(terms, {}, &Term::term));
  CHECK(std::ranges::adjacent_find(terms, {}, &Term::term) == terms.end());
  for (const auto& t : terms) {
    CHECK_FALSE(t.postings.empty());
    for (const auto& p : t.postings) {
      const auto i = static_cast<std::size_t>(p.index);
      switch (p.target) {
        case Target::Entry: CHECK(i < data::entries().size()); break;
        case Target::Recipe: CHECK(i < data::recipes().size()); break;
        case Target::Compare: CHECK(i < data::compares().size()); break;
        case Target::Index:
          REQUIRE(i < data::index_names().size());
          CHECK(p.field == Field::Name);
          CHECK(data::index_names()[i].entry < 0);  // a card stands for covered names
          break;
      }
    }
  }
  auto find = [&](std::string_view s) {
    auto it = std::ranges::lower_bound(terms, s, {}, &Term::term);
    return it != terms.end() && it->term == s ? &*it : nullptr;
  };
  const Term* phrase = find("remove duplicates");
  REQUIRE(phrase);
  CHECK(phrase->postings.front().field == Field::Phrase);
  CHECK_FALSE(find("the"));  // stop words are not indexed
}

TEST_CASE("the changelog section of this version is embedded") {
  CHECK(data::changelog().find("### ") != std::string_view::npos);
}
