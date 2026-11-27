// The fuzzy scorer for "did you mean" and the finder (PLAN §8.2).
#pragma once

#include <string_view>

namespace cxxman::search {

// Case-insensitive; 0 means no match. The best of: a prefix match
// (1000 - extra length), an edit distance d <= max(1, len/3) (500 - 100 d),
// or a subsequence match (200 + bonuses - gaps). Both the whole key and its
// part after the last "::" are tried, so "emplce_back" finds
// "vector::emplace_back".
int fuzzy_score(std::string_view query, std::string_view key);

// Optimal string alignment distance (Damerau-Levenshtein with adjacent
// transpositions), case-insensitive. Returns bound + 1 once it exceeds bound.
int edit_distance(std::string_view a, std::string_view b, int bound);

}  // namespace cxxman::search
