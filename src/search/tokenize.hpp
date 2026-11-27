// Words for task search (PLAN §8.3). cxxman-gen tokenizes the content with
// this to build the postings table, and the search tokenizes the query with
// it, so both sides always agree.
#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace cxxman::search {

// Words too common to say anything about a task; "std" is on every name.
inline constexpr std::array<std::string_view, 25> stop_words{
    "a",  "an", "and", "are", "as", "at", "be",  "by",  "do", "for", "from", "how", "i",
    "in", "into", "is", "it", "my", "of", "on", "or", "std", "the", "to", "with"};

constexpr bool is_stop_word(std::string_view w) {
  return std::ranges::find(stop_words, w) != stop_words.end();
}

// Lowercase ASCII words, split at anything but letters and digits ("::", "-",
// "_", spaces, punctuation), without stop words. Bytes above 0x7F stay part
// of a word.
inline std::vector<std::string> tokenize(std::string_view text) {
  std::vector<std::string> out;
  std::string cur;
  auto flush = [&] {
    if (!cur.empty() && !is_stop_word(cur)) out.push_back(cur);
    cur.clear();
  };
  for (char c : text) {
    const auto u = static_cast<unsigned char>(c);
    if (c >= 'A' && c <= 'Z')
      cur += static_cast<char>(c - 'A' + 'a');
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || u >= 0x80)
      cur += c;
    else
      flush();
  }
  flush();
  return out;
}

// The words of `text` joined by single spaces: the key of a whole tag phrase.
inline std::string phrase(std::string_view text) {
  std::string out;
  for (const auto& w : tokenize(text)) {
    if (!out.empty()) out += ' ';
    out += w;
  }
  return out;
}

}  // namespace cxxman::search
