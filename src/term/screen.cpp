#include "term/screen.hpp"

#include <utility>

namespace cxxman::term {

std::string move_to(std::size_t row, std::size_t col) {
  return "\x1b[" + std::to_string(row) + ";" + std::to_string(col) + "H";
}

std::string Screen::update(std::vector<std::string> rows) {
  std::string out;
  // After invalidate() the screen may hold anything: clear it once.
  if (full_) out += "\x1b[0m\x1b[2J";
  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (!full_ && i < last_.size() && last_[i] == rows[i]) continue;
    out += move_to(i + 1, 1) + rows[i] + "\x1b[0m\x1b[K";
  }
  for (std::size_t i = rows.size(); i < last_.size(); ++i)
    if (!last_[i].empty()) out += move_to(i + 1, 1) + "\x1b[K";
  last_ = std::move(rows);
  full_ = false;
  return out;
}

}  // namespace cxxman::term
