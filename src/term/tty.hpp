// A full-screen terminal for the pager and the finder (PLAN §4.6, §4.8): an
// interface, so the tests can drive both with a fake one and scripted keys.
#pragma once

#include "term/keys.hpp"

#include <cstddef>
#include <memory>
#include <string_view>

namespace cxxman::term {

struct Size {
  std::size_t rows = 0, cols = 0;
};

class Tty {
 public:
  virtual ~Tty() = default;
  virtual Size size() = 0;
  // Blocks until a key arrives; Resize when the window changed, Eof when the
  // input went away.
  virtual Key read() = 0;
  virtual void write(std::string_view s) = 0;
};

// The real terminal in full-screen mode: the alternate screen, raw input,
// no cursor and no line wrap. Destroying it puts everything back, and so do
// Ctrl-Break / closing the window (Windows) and signals (POSIX), so the
// terminal is restored on every exit path. Input comes from the console
// (CONIN$, /dev/tty) even when stdin is redirected. nullptr when there is no
// terminal to use. One at a time.
std::unique_ptr<Tty> open_fullscreen();

}  // namespace cxxman::term
