#include "tui/finder_model.hpp"

#include "search/fuzzy.hpp"
#include "search/search.hpp"

#include <algorithm>
#include <string>

namespace cxxman::tui {

namespace {

using K = Item::Kind;

// Cards first, then recipes and compares, then index names (as §8.3 ties).
int rank_of(K k) {
  switch (k) {
    case K::Card: return 0;
    case K::Recipe: case K::Compare: return 1;
    case K::Name: return 2;
    case K::Header: break;
  }
  return 3;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
  while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
  return s;
}

bool starts_with_std(std::string_view s) {
  if (s.size() < 5) return false;
  std::string head(s.substr(0, 5));
  for (char& c : head)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return head == "std::";
}

}  // namespace

FinderModel::FinderModel(const Catalog& cat) : cat_(&cat) {
  auto add = [&](K kind, std::int32_t index, std::string_view name, std::string_view header) {
    items_.push_back({kind, index, name, header});
    return static_cast<std::int32_t>(items_.size() - 1);
  };
  entry_item_.resize(cat.entries.size());
  for (std::size_t i = 0; i < cat.entries.size(); ++i)
    entry_item_[i] = add(K::Card, static_cast<std::int32_t>(i), cat.entries[i].id, cat.entries[i].header);
  recipe_item_.resize(cat.recipes.size());
  for (std::size_t i = 0; i < cat.recipes.size(); ++i)
    recipe_item_[i] = add(K::Recipe, static_cast<std::int32_t>(i), cat.recipes[i].id, {});
  compare_item_.resize(cat.compares.size());
  for (std::size_t i = 0; i < cat.compares.size(); ++i)
    compare_item_[i] = add(K::Compare, static_cast<std::int32_t>(i), cat.compares[i].id, {});
  // Index names without a card, once each (a card stands for the names it covers).
  for (std::size_t i = 0; i < cat.index.size(); ++i) {
    const IndexName& n = cat.index[i];
    if (n.entry >= 0) continue;
    auto it = std::ranges::lower_bound(names_, n.name, {}, &KeyRef::key);
    if (it != names_.end() && it->key == n.name) continue;
    const auto item = static_cast<std::uint32_t>(add(K::Name, static_cast<std::int32_t>(i), n.name, n.header));
    names_.insert(it, {n.name, item});
  }
  headers_from_ = items_.size();
  std::vector<std::string_view> headers;
  for (const auto& n : known_names(cat)) headers.push_back(n.header);
  for (const auto& h : cat.headers) headers.push_back(h.header);
  std::ranges::sort(headers);
  const auto dup = std::ranges::unique(headers);
  headers.erase(dup.begin(), dup.end());
  for (auto h : headers) add(K::Header, -1, h, {});

  for (const Key& k : cat.keys) {
    std::int32_t item = -1;
    const auto i = static_cast<std::size_t>(k.index);
    switch (k.target) {
      case Target::Entry: item = entry_item_[i]; break;
      case Target::Recipe: item = recipe_item_[i]; break;
      case Target::Compare: item = compare_item_[i]; break;
      case Target::Index: {
        if (cat.index[i].entry >= 0) break;
        auto it = std::ranges::lower_bound(names_, cat.index[i].name, {}, &KeyRef::key);
        if (it != names_.end() && it->key == cat.index[i].name) item = static_cast<std::int32_t>(it->item);
        break;
      }
    }
    if (item >= 0) keys_.push_back({k.key, static_cast<std::uint32_t>(item)});
  }
  for (std::size_t i = headers_from_; i < items_.size(); ++i)
    keys_.push_back({items_[i].name, static_cast<std::uint32_t>(i)});
}

std::vector<Match> FinderModel::rank(std::string_view q, Item::Kind only, bool any) const {
  auto wanted = [&](const Item& it) { return any ? it.kind != K::Header : it.kind == only; };
  std::vector<Match> out;
  if (q.empty()) {
    for (std::size_t i = 0; i < items_.size(); ++i)
      if (wanted(items_[i])) out.push_back({static_cast<std::uint32_t>(i), 0});
    return out;
  }
  std::vector<int> best(items_.size(), 0);
  for (const auto& k : keys_) {
    if (!wanted(items_[k.item])) continue;
    best[k.item] = std::max(best[k.item], search::fuzzy_score(q, k.key));
  }
  for (std::size_t i = 0; i < best.size(); ++i)
    if (best[i] > 0) out.push_back({static_cast<std::uint32_t>(i), best[i]});
  std::ranges::sort(out, [&](const Match& a, const Match& b) {
    if (a.score != b.score) return a.score > b.score;
    const Item &x = items_[a.item], &y = items_[b.item];
    if (rank_of(x.kind) != rank_of(y.kind)) return rank_of(x.kind) < rank_of(y.kind);
    if (x.name.size() != y.name.size()) return x.name.size() < y.name.size();
    return x.name < y.name;
  });
  return out;
}

std::vector<Match> FinderModel::tasks(std::string_view q) const {
  std::vector<Match> out;
  for (const auto& h : search::search(*cat_, q, 100)) {
    std::int32_t item = -1;
    const auto i = static_cast<std::size_t>(h.index);
    switch (h.target) {
      case Target::Entry: item = entry_item_[i]; break;
      case Target::Recipe: item = recipe_item_[i]; break;
      case Target::Compare: item = compare_item_[i]; break;
      case Target::Index: {
        auto it = std::ranges::lower_bound(names_, cat_->index[i].name, {}, &KeyRef::key);
        if (it != names_.end() && it->key == cat_->index[i].name) item = static_cast<std::int32_t>(it->item);
        break;
      }
    }
    if (item >= 0) out.push_back({static_cast<std::uint32_t>(item), h.score});
  }
  return out;
}

std::vector<Match> FinderModel::find(std::string_view query) const {
  std::string_view q = trim(query);
  if (q.starts_with('<')) {
    q = trim(q.substr(1));
    if (q.ends_with('>')) q.remove_suffix(1);
    return rank(q, K::Header, false);
  }
  if (q.starts_with('#')) return rank(trim(q.substr(1)), K::Recipe, false);
  if (q.starts_with('=')) return rank(trim(q.substr(1)), K::Compare, false);
  if (q.find(' ') != std::string_view::npos) return tasks(q);
  if (starts_with_std(q)) q.remove_prefix(5);
  return rank(q, K::Card, true);
}

}  // namespace cxxman::tui
