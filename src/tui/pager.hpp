// The built-in pager (PLAN §4.8): long output one screen at a time. The state
// machine is apart from the terminal, so the tests drive it with keys.
#pragma once

#include "term/keys.hpp"
#include "term/tty.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace cxxman::tui {

class Pager {
 public:
  // `text`: painted lines (escape codes allowed), each ended by '\n'. It
  // must outlive the pager.
  explicit Pager(std::string_view text);

  // Handles one key on a screen of `rows` rows; false when the pager closes.
  bool handle(const term::Key& k, std::size_t rows);

  // Everything to write for one screen: the text rows and the status line.
  std::string frame(term::Size s) const;

  // The status line as plain text, for `cols` columns.
  std::string status(term::Size s) const;

  std::size_t top() const { return top_; }
  std::size_t line_count() const { return lines_.size(); }

 private:
  std::size_t body(std::size_t rows) const { return rows > 1 ? rows - 1 : 1; }
  std::size_t max_top(std::size_t rows) const;
  void scroll(std::ptrdiff_t by, std::size_t rows);
  bool search(std::size_t from, bool forward, std::size_t rows);
  std::string row(std::size_t line) const;

  std::vector<std::string_view> lines_;  // as painted
  std::vector<std::string> plain_;       // without escape codes
  std::vector<std::string> folded_;      // plain_, ASCII-lowercased, for search
  std::size_t top_ = 0;
  std::size_t match_ = static_cast<std::size_t>(-1);  // the line of the last search hit
  bool typing_ = false;  // reading a search after '/'
  std::string input_;    // what is typed after '/'
  std::string query_;    // the active search, lowercased
  std::string message_;  // shown once in the status line
};

// Shows `text` on `tty` until the user quits (q, Esc, Ctrl-C) or the input
// goes away. `tty` is already full-screen; the caller restores it.
void page(term::Tty& tty, std::string_view text);

}  // namespace cxxman::tui
