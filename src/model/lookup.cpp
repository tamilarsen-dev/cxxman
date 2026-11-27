#include "model/lookup.hpp"

#include "search/fuzzy.hpp"

#include <algorithm>
#include <map>
#include <tuple>

namespace cxxman {

namespace {

// Suggestions below this score are noise (long gappy subsequences), and so
// are those far below the best one: with a one-typo match (400) around, a
// loose subsequence match (~250) only distracts.
constexpr int min_suggestion_score = 150;
constexpr int suggestion_window = 100;

std::span<const Key> keys_folded(const Catalog& cat, std::string_view folded) {
  auto [first, last] = std::ranges::equal_range(cat.keys, folded, {}, &Key::folded);
  return {first, last};
}

bool is_class(const Catalog& cat, std::string_view name) {
  for (const Key& k : keys_folded(cat, fold(name))) {
    if (k.key != name) continue;
    const auto i = static_cast<std::size_t>(k.index);
    if (k.target == Target::Entry && cat.entries[i].kind == Kind::Class) return true;
    if (k.target == Target::Index && cat.index[i].kind == Kind::Class) return true;
  }
  return false;
}

// Names "<class>::<name>" whose last part equals `name` (case-insensitive).
std::vector<std::string_view> members_named(const Catalog& cat, std::string_view folded_name,
                                            std::int32_t exclude_entry) {
  std::vector<std::string_view> out;
  for (const Key& k : cat.keys) {
    if (k.target != Target::Entry && k.target != Target::Index) continue;
    const auto sep = k.folded.rfind("::");
    if (sep == std::string_view::npos || k.folded.substr(sep + 2) != folded_name) continue;
    if (k.target == Target::Entry && k.index == exclude_entry) continue;
    if (!is_class(cat, k.key.substr(0, sep))) continue;
    out.push_back(k.key);
  }
  std::ranges::sort(out);
  const auto dup = std::ranges::unique(out);
  out.erase(dup.begin(), dup.end());
  return out;
}

}  // namespace

Catalog Catalog::builtin() noexcept {
  return {data::entries(), data::index_names(), data::keys()};
}

std::string fold(std::string_view s) {
  std::string out(s);
  for (auto& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

std::string normalize_name(std::string_view input) {
  const auto first = input.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) return {};
  input = input.substr(first, input.find_last_not_of(" \t\r\n") - first + 1);
  if (input.starts_with("std::")) input.remove_prefix(5);
  std::string out;
  for (char c : input) {
    if (c == '.')
      out += "::";
    else
      out += c;
  }
  return out;
}

std::int32_t card_for(const Catalog& cat, std::string_view name) {
  const std::string q = normalize_name(name);
  for (const Key& k : keys_folded(cat, fold(q)))
    if (k.target == Target::Entry && k.key == q) return k.index;
  return -1;
}

Resolution resolve(const Catalog& cat, std::string_view input) {
  Resolution r;
  const std::string q = normalize_name(input);
  if (q.empty()) return r;
  const std::string f = fold(q);
  const auto range = keys_folded(cat, f);

  // Cards: exact spelling first, then any case.
  for (const Key& k : range)
    if (k.target == Target::Entry && k.key == q) {
      r.entry = k.index;
      break;
    }
  if (r.entry < 0)
    for (const Key& k : range)
      if (k.target == Target::Entry) {
        r.entry = k.index;
        break;
      }

  // Index names without a card, same order. A name can have several rows.
  std::string_view index_name;
  for (const Key& k : range)
    if (k.target == Target::Index && k.key == q) index_name = k.key;
  if (index_name.empty())
    for (const Key& k : range)
      if (k.target == Target::Index) {
        index_name = k.key;
        break;
      }
  if (!index_name.empty())
    for (const Key& k : range)
      if (k.target == Target::Index && k.key == index_name) r.rows.push_back(k.index);

  const bool bare = q.find("::") == std::string::npos;
  const auto members = bare ? members_named(cat, f, r.entry) : std::vector<std::string_view>{};

  if (r.entry >= 0) {
    r.what = Resolution::What::Card;
    r.also = members;
  } else if (!r.rows.empty()) {
    r.what = Resolution::What::Index;
    r.also = members;
  } else if (members.size() >= 2) {
    r.what = Resolution::What::Ambiguous;
    r.also = members;
  } else if (members.size() == 1) {
    return resolve(cat, members.front());
  } else {
    r.suggestions = suggest(cat, q);
  }
  return r;
}

std::vector<std::string_view> suggest(const Catalog& cat, std::string_view query, std::size_t max) {
  struct Hit {
    int score;
    int rank;  // cards before index names
    std::string_view key;
  };
  // One hit per card (its best key) and one per index name.
  std::map<std::int32_t, Hit> cards;
  std::map<std::string_view, Hit> names;
  for (const Key& k : cat.keys) {
    if (k.target != Target::Entry && k.target != Target::Index) continue;
    const int score = search::fuzzy_score(query, k.key);
    if (score < min_suggestion_score) continue;
    const Hit hit{score, k.target == Target::Entry ? 0 : 2, k.key};
    // Per card, the best key; on a tie the shortest (the id over ranges::id).
    auto& slot = k.target == Target::Entry ? cards[k.index] : names[k.key];
    if (slot.key.empty() || slot.score < score ||
        (slot.score == score && k.key.size() < slot.key.size()))
      slot = hit;
  }
  std::vector<Hit> hits;
  for (auto& [_, h] : cards) hits.push_back(h);
  for (auto& [name, h] : names) {
    // An index name that a card covers is already there as the card.
    if (card_for(cat, name) < 0) hits.push_back(h);
  }
  std::ranges::sort(hits, [](const Hit& a, const Hit& b) {
    return std::tuple(-a.score, a.rank, a.key.size(), a.key) <
           std::tuple(-b.score, b.rank, b.key.size(), b.key);
  });
  std::vector<std::string_view> out;
  for (const auto& h : hits) {
    if (out.size() == max || h.score < hits.front().score - suggestion_window) break;
    if (std::ranges::find(out, h.key) == out.end()) out.push_back(h.key);
  }
  return out;
}

}  // namespace cxxman
