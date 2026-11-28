// The screen diff (PLAN §10): only changed rows are written.
#include "term/screen.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using cxxman::term::Screen;

TEST_CASE("screen: the first frame clears and writes every row") {
  Screen s;
  CHECK(s.update({"a", "b"}) == "\x1b[0m\x1b[2J\x1b[1;1Ha\x1b[0m\x1b[K\x1b[2;1Hb\x1b[0m\x1b[K");
}

TEST_CASE("screen: an unchanged frame writes nothing, a change writes its row") {
  Screen s;
  s.update({"a", "b", "c"});
  CHECK(s.update({"a", "b", "c"}).empty());
  CHECK(s.update({"a", "B", "c"}) == "\x1b[2;1HB\x1b[0m\x1b[K");
  // Colors count as a change: the same text painted differently.
  CHECK(s.update({"a", "\x1b[7mB\x1b[27m", "c"}) == "\x1b[2;1H\x1b[7mB\x1b[27m\x1b[0m\x1b[K");
}

TEST_CASE("screen: rows that went away are cleared, new ones written") {
  Screen s;
  s.update({"a", "b", "", "d"});
  CHECK(s.update({"a"}) == "\x1b[2;1H\x1b[K\x1b[4;1H\x1b[K");  // row 3 was empty already
  CHECK(s.update({"a", "x"}) == "\x1b[2;1Hx\x1b[0m\x1b[K");
}

TEST_CASE("screen: invalidate redraws everything") {
  Screen s;
  s.update({"a", "b"});
  s.invalidate();
  CHECK(s.update({"a", "b"}) == "\x1b[0m\x1b[2J\x1b[1;1Ha\x1b[0m\x1b[K\x1b[2;1Hb\x1b[0m\x1b[K");
  CHECK(s.update({"a", "b"}).empty());
}
