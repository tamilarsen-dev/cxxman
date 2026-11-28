// The screen diff (PLAN §4.6): the finder builds whole frames, and only the
// rows that changed since the last frame are written, so nothing flickers.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace cxxman::term {

class Screen {
 public:
  // What to write to turn the last frame into `rows` (one painted line per
  // screen row, top first): each changed row is moved to, written, its colors
  // reset and the rest of it cleared; rows the last frame had beyond
  // `rows.size()` are cleared.
  std::string update(std::vector<std::string> rows);

  // Forget the last frame, so the next update writes every row (after a
  // resize, or when something else drew on the screen).
  void invalidate() {
    last_.clear();
    full_ = true;
  }

 private:
  std::vector<std::string> last_;
  bool full_ = true;  // the screen holds nothing we know of
};

// ESC [ row ; col H, both from 1.
std::string move_to(std::size_t row, std::size_t col);

}  // namespace cxxman::term
