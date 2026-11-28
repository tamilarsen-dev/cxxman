// A terminal for tests (PLAN §10): a fixed size, scripted keys, and
// everything written kept. A Resize key makes the screen 5 rows taller.
#pragma once

#include "term/tty.hpp"

#include <deque>
#include <string>
#include <string_view>
#include <utility>

namespace cxxman::test {

class FakeTty final : public term::Tty {
 public:
  FakeTty(term::Size s, std::deque<term::Key> keys) : size_(s), keys_(std::move(keys)) {}
  term::Size size() override { return size_; }
  term::Key read() override {
    if (keys_.empty()) return term::key(term::Key::Code::Eof);
    const term::Key k = keys_.front();
    keys_.pop_front();
    if (k.code == term::Key::Code::Resize) size_ = {size_.rows + 5, size_.cols};
    return k;
  }
  void write(std::string_view s) override {
    written += s;
    ++frames;
  }
  std::size_t left() const { return keys_.size(); }

  std::string written;
  int frames = 0;

 private:
  term::Size size_;
  std::deque<term::Key> keys_;
};

}  // namespace cxxman::test
