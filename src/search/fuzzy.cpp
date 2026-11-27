#include "search/fuzzy.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace cxxman::search {

namespace {

constexpr char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }
constexpr bool is_lower(char c) { return c >= 'a' && c <= 'z'; }
constexpr bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }

bool word_start(std::string_view key, std::size_t p) {
  if (p == 0) return true;
  const char prev = key[p - 1];
  return prev == '_' || prev == ':' || prev == '<' || (is_lower(prev) && is_upper(key[p]));
}

int subsequence(std::string_view q, std::string_view key) {
  std::size_t j = 0, first = 0, last = 0, matched = 0;
  int starts = 0, runs = 0;
  for (std::size_t p = 0; p < key.size() && j < q.size(); ++p) {
    if (lower(key[p]) != lower(q[j])) continue;
    if (matched == 0) first = p;
    if (word_start(key, p)) ++starts;
    if (matched > 0 && p == last + 1) ++runs;
    last = p;
    ++matched;
    ++j;
  }
  if (j < q.size()) return 0;
  const auto gaps = static_cast<int>(last - first + 1 - matched);
  return 200 + 15 * starts + 5 * runs - gaps;
}

int score_one(std::string_view q, std::string_view key) {
  if (q.empty() || key.empty()) return 0;
  int best = 0;
  if (key.size() >= q.size() &&
      std::equal(q.begin(), q.end(), key.begin(), [](char a, char b) { return lower(a) == lower(b); }))
    best = 1000 - static_cast<int>(key.size() - q.size());
  const int bound = std::max(1, static_cast<int>(q.size()) / 3);
  const auto diff = static_cast<int>(key.size()) - static_cast<int>(q.size());
  if (best == 0 && diff <= bound && -diff <= bound) {
    const int d = edit_distance(q, key, bound);
    if (d <= bound) best = 500 - 100 * d;
  }
  return std::max(best, subsequence(q, key));
}

}  // namespace

int edit_distance(std::string_view a, std::string_view b, int bound) {
  const std::size_t n = a.size(), m = b.size();
  // Three rows: i-2, i-1, i.
  std::vector<int> prev2(m + 1), prev(m + 1), cur(m + 1);
  for (std::size_t j = 0; j <= m; ++j) prev[j] = static_cast<int>(j);
  for (std::size_t i = 1; i <= n; ++i) {
    cur[0] = static_cast<int>(i);
    int row_min = cur[0];
    for (std::size_t j = 1; j <= m; ++j) {
      const int cost = lower(a[i - 1]) == lower(b[j - 1]) ? 0 : 1;
      cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
      if (i > 1 && j > 1 && lower(a[i - 1]) == lower(b[j - 2]) && lower(a[i - 2]) == lower(b[j - 1]))
        cur[j] = std::min(cur[j], prev2[j - 2] + 1);
      row_min = std::min(row_min, cur[j]);
    }
    if (row_min > bound) return bound + 1;
    std::swap(prev2, prev);
    std::swap(prev, cur);
  }
  return std::min(prev[m], bound + 1);
}

int fuzzy_score(std::string_view query, std::string_view key) {
  int best = score_one(query, key);
  const auto sep = key.rfind("::");
  if (sep != std::string_view::npos) best = std::max(best, score_one(query, key.substr(sep + 2)));
  return best;
}

}  // namespace cxxman::search
