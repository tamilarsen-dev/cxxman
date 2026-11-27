#include "search/search.hpp"

#include "search/tokenize.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <tuple>
#include <utility>

namespace cxxman::search {

namespace {

// PLAN §8.3 weights, doubled (see search.hpp).
constexpr int prefix_weight = 10;
constexpr int phrase_weight = 50;
constexpr std::size_t min_prefix = 3;

constexpr int weight(Field f) {
  switch (f) {
    case Field::Id: return 60;
    case Field::Tag: return 30;
    case Field::Text: return 16;
    case Field::Name: return 20;
    case Field::Phrase: break;
  }
  return 0;
}

constexpr int rank(Target t) {
  switch (t) {
    case Target::Entry: return 0;
    case Target::Recipe:
    case Target::Compare: return 1;
    case Target::Index: break;
  }
  return 2;
}

struct Doc {
  std::vector<int> best;  // per query word
  int bonus = 0;
};

}  // namespace

std::string_view hit_name(const Catalog& cat, const Hit& h) {
  const auto i = static_cast<std::size_t>(h.index);
  switch (h.target) {
    case Target::Entry: return cat.entries[i].id;
    case Target::Recipe: return cat.recipes[i].id;
    case Target::Compare: return cat.compares[i].id;
    case Target::Index: break;
  }
  return cat.index[i].name;
}

std::vector<Hit> search(const Catalog& cat, std::string_view query, std::size_t max) {
  const auto words = tokenize(query);
  if (words.empty()) return {};

  // Index rows with one name are one document, keyed by the name's first row.
  auto first_row = [&](std::int32_t row) {
    const auto name = cat.index[static_cast<std::size_t>(row)].name;
    return static_cast<std::int32_t>(
        std::ranges::lower_bound(cat.index, name, {}, &IndexName::name) - cat.index.begin());
  };
  std::map<std::pair<Target, std::int32_t>, Doc> docs;
  auto doc = [&](const Posting& p) -> Doc& {
    const std::int32_t i = p.target == Target::Index ? first_row(p.index) : p.index;
    Doc& d = docs[{p.target, i}];
    if (d.best.empty()) d.best.assign(words.size(), 0);
    return d;
  };

  for (std::size_t w = 0; w < words.size(); ++w) {
    const std::string& word = words[w];
    // The exact term sorts first, then the longer terms it is a prefix of.
    for (auto it = std::ranges::lower_bound(cat.terms, word, {}, &Term::term);
         it != cat.terms.end() && it->term.starts_with(word); ++it) {
      const bool exact = it->term.size() == word.size();
      if (!exact && word.size() < min_prefix) break;
      for (const Posting& p : it->postings) {
        if (p.field == Field::Phrase || (!exact && p.field == Field::Name)) continue;
        Doc& d = doc(p);
        d.best[w] = std::max(d.best[w], exact ? weight(p.field) : prefix_weight);
      }
    }
  }

  std::string whole;
  for (const auto& w : words) whole += (whole.empty() ? "" : " ") + w;
  if (auto it = std::ranges::lower_bound(cat.terms, whole, {}, &Term::term);
      it != cat.terms.end() && it->term == whole)
    for (const Posting& p : it->postings)
      if (p.field == Field::Phrase) doc(p).bonus = phrase_weight;

  std::vector<Hit> hits;
  for (const auto& [key, d] : docs) {
    int sum = d.bonus;
    bool all = true;
    for (int b : d.best) {
      sum += b;
      all = all && b > 0;
    }
    Hit h{key.first, key.second, all ? sum * 3 / 2 : sum, 1};
    if (h.target == Target::Index) {
      const auto rows = std::ranges::equal_range(cat.index, hit_name(cat, h), {}, &IndexName::name);
      std::vector<std::string_view> headers;
      for (const auto& r : rows) headers.push_back(r.header);
      std::ranges::sort(headers);
      h.headers = static_cast<std::size_t>(std::ranges::unique(headers).begin() - headers.begin());
    }
    hits.push_back(h);
  }
  std::ranges::sort(hits, [&](const Hit& a, const Hit& b) {
    const auto na = hit_name(cat, a), nb = hit_name(cat, b);
    return std::tuple(-a.score, rank(a.target), na.size(), na) <
           std::tuple(-b.score, rank(b.target), nb.size(), nb);
  });
  if (hits.size() > max) hits.resize(max);
  return hits;
}

}  // namespace cxxman::search
