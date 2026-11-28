// The built-in pager (PLAN §4.8), driven with keys and a fake terminal.
#include "tui/pager.hpp"

#include "fake_tty.hpp"

#include <doctest/doctest.h>

#include <string>
#include <string_view>

using namespace cxxman;
using term::chr;
using term::ctrl;
using term::key;
using C = term::Key::Code;

namespace {

// "line 1\n" ... "line n\n".
std::string numbered(int n) {
  std::string s;
  for (int i = 1; i <= n; ++i) s += "line " + std::to_string(i) + "\n";
  return s;
}

using test::FakeTty;

constexpr std::size_t rows = 11;  // 10 text rows and the status line

}  // namespace

TEST_CASE("pager pages, scrolls and stops at both ends") {
  const std::string text = numbered(35);
  tui::Pager p(text);
  CHECK(p.line_count() == 35);
  CHECK(p.handle(chr(' '), rows));
  CHECK(p.top() == 10);
  CHECK(p.handle(key(C::PageDown), rows));
  CHECK(p.top() == 20);
  CHECK(p.handle(ctrl('f'), rows));
  CHECK(p.top() == 25);  // the last screen ends on the last line
  CHECK(p.handle(chr(' '), rows));
  CHECK(p.top() == 25);
  CHECK(p.handle(chr('b'), rows));
  CHECK(p.top() == 15);
  CHECK(p.handle(chr('k'), rows));
  CHECK(p.top() == 14);
  CHECK(p.handle(key(C::Up), rows));
  CHECK(p.top() == 13);
  CHECK(p.handle(chr('j'), rows));
  CHECK(p.handle(key(C::Down), rows));
  CHECK(p.handle(key(C::Enter), rows));
  CHECK(p.top() == 16);
  CHECK(p.handle(chr('d'), rows));
  CHECK(p.top() == 21);
  CHECK(p.handle(chr('u'), rows));
  CHECK(p.top() == 16);
  CHECK(p.handle(chr('g'), rows));
  CHECK(p.top() == 0);
  CHECK(p.handle(key(C::PageUp), rows));
  CHECK(p.top() == 0);
  CHECK(p.handle(chr('G'), rows));
  CHECK(p.top() == 25);
  CHECK(p.handle(key(C::Home), rows));
  CHECK(p.top() == 0);
  CHECK(p.handle(key(C::End), rows));
  CHECK(p.top() == 25);
  // A taller window after a resize keeps the last screen full.
  CHECK(p.handle(key(C::Resize), 21));
  CHECK(p.top() == 15);
}

TEST_CASE("pager closes on q, Esc and Ctrl-C, and ignores other keys") {
  const std::string text = numbered(30);
  for (auto k : {chr('q'), chr('Q'), key(C::Esc), ctrl('c')}) {
    tui::Pager p(text);
    CHECK_FALSE(p.handle(k, rows));
  }
  tui::Pager p(text);
  CHECK(p.handle(chr('x'), rows));
  CHECK(p.handle(key(C::Tab), rows));
  CHECK(p.handle(ctrl('z'), rows));
  CHECK(p.top() == 0);
}

TEST_CASE("pager search finds, repeats and reports misses") {
  const std::string text = numbered(40);
  tui::Pager p(text);
  // "/line 3" then Enter: the first hit is line 3 (index 2).
  for (auto k : {chr('/'), chr('L'), chr('i'), chr('n'), chr('e'), chr(' '), chr('3')}) CHECK(p.handle(k, rows));
  CHECK(p.status({rows, 80}) == "/Line 3");
  CHECK(p.handle(key(C::Enter), rows));
  CHECK(p.top() == 2);
  CHECK(p.status({rows, 80}).find("/line 3") != std::string::npos);  // case-folded
  CHECK(p.handle(chr('n'), rows));
  CHECK(p.top() == 29);  // "line 30"
  CHECK(p.handle(chr('n'), rows));
  CHECK(p.top() == 30);  // "line 31": the screen can still move down
  CHECK(p.handle(chr('N'), rows));
  CHECK(p.top() == 29);
  CHECK(p.handle(chr('N'), rows));
  CHECK(p.top() == 2);
  CHECK(p.handle(chr('N'), rows));
  CHECK(p.top() == 2);
  CHECK(p.status({rows, 80}).starts_with(" no more matches for line 3"));
  // Hits past the last full screen leave the screen at the end, and n moves
  // on from the hit, not from the top.
  CHECK(p.handle(chr('G'), rows));
  for (auto k : {chr('/'), chr('i'), chr('n'), chr('e'), chr(' '), chr('3'), key(C::Enter)}) p.handle(k, rows);
  CHECK(p.top() == 30);
  for (int i = 0; i < 9; ++i) CHECK(p.handle(chr('n'), rows));
  CHECK(p.top() == 30);
  CHECK(p.status({rows, 80}).starts_with(" no more matches"));
  // A miss keeps the place.
  for (auto k : {chr('/'), chr('z'), key(C::Enter)}) p.handle(k, rows);
  CHECK(p.top() == 30);
  CHECK(p.status({rows, 80}).starts_with(" not found: z"));
  // The message shows once.
  CHECK(p.handle(chr('x'), rows));
  CHECK(p.status({rows, 80}).starts_with(" lines 31-40 of 40  (END)"));
}

TEST_CASE("pager search input: backspace, cancel, empty repeats") {
  const std::string text = numbered(40);
  tui::Pager p(text);
  for (auto k : {chr('/'), chr('x'), chr('y'), key(C::Backspace)}) p.handle(k, rows);
  CHECK(p.status({rows, 80}) == "/x");
  p.handle(key(C::Esc), rows);  // cancels the search, not the pager
  CHECK(p.status({rows, 80}).starts_with(" lines 1-10 of 40"));
  for (auto k : {chr('/'), chr(U'é'), key(C::Backspace)}) p.handle(k, rows);
  CHECK(p.status({rows, 80}) == "/");
  p.handle(key(C::Backspace), rows);  // backspace on nothing closes the prompt
  CHECK(p.status({rows, 80}).starts_with(" lines"));
  for (auto k : {chr('/'), chr('4'), chr('0'), key(C::Enter)}) p.handle(k, rows);
  CHECK(p.top() == 30);
  p.handle(chr('g'), rows);
  for (auto k : {chr('/'), key(C::Enter)}) p.handle(k, rows);  // repeats "40"
  CHECK(p.top() == 30);
  for (auto k : {chr('/'), chr('a'), ctrl('c')}) CHECK(p.handle(k, rows));  // Ctrl-C cancels typing
  CHECK(p.status({rows, 80}).find("/40") != std::string::npos);
}

TEST_CASE("pager status line: position, percent, help when it fits") {
  const std::string text = numbered(40);
  tui::Pager p(text);
  const std::string wide = p.status({rows, 80});
  CHECK(wide.size() == 80);
  CHECK(wide.starts_with(" lines 1-10 of 40  25%"));
  CHECK(wide.ends_with("q quit  / search  space b page "));
  CHECK(p.status({rows, 30}) == " lines 1-10 of 40  25%");
  p.handle(chr('G'), rows);
  CHECK(p.status({rows, 30}) == " lines 31-40 of 40  (END)");
}

TEST_CASE("pager frame: rows, cleared ends, reverse status, search hits") {
  const std::string text = "\x1b[1msort\x1b[0m <algorithm>\nSort a range.\n";
  tui::Pager p(text);
  const std::string f = p.frame({4, 40});
  CHECK(f.starts_with("\x1b[1;1H\x1b[1msort\x1b[0m <algorithm>\x1b[0m\x1b[K"));
  CHECK(f.find("\x1b[2;1HSort a range.\x1b[0m\x1b[K") != std::string::npos);
  CHECK(f.find("\x1b[3;1H\x1b[0m\x1b[K") != std::string::npos);  // past the end: blank
  CHECK(f.find("\x1b[4;1H\x1b[7m lines 1-2 of 2  (END)") != std::string::npos);
  CHECK(f.ends_with("\x1b[0m\x1b[?25l"));
  // A hit is shown on the plain text, so color codes never split it.
  for (auto k : {chr('/'), chr('S'), chr('O'), chr('R'), chr('T'), key(C::Enter)}) p.handle(k, 4);
  const std::string g = p.frame({4, 40});
  CHECK(g.starts_with("\x1b[1;1H\x1b[7msort\x1b[27m <algorithm>\x1b[0m\x1b[K"));
  CHECK(g.find("\x1b[2;1H\x1b[7mSort\x1b[27m a range.") != std::string::npos);
  // While typing, the cursor shows after the prompt.
  p.handle(chr('/'), 4);
  p.handle(chr('a'), 4);
  CHECK(p.frame({4, 40}).ends_with("\x1b[4;3H\x1b[?25h"));
}

TEST_CASE("pager strips OSC 8 links and CRLF for search") {
  const std::string text = "\x1b]8;;https://en.cppreference.com/w/cpp/algorithm/sort\x1b\\sort\x1b]8;;\x1b\\ x\r\n";
  tui::Pager p(text);
  for (auto k : {chr('/'), chr('t'), chr(' '), chr('x'), key(C::Enter)}) p.handle(k, 4);
  CHECK(p.frame({4, 40}).starts_with("\x1b[1;1Hsor\x1b[7mt x\x1b[27m\x1b[0m\x1b[K"));
}

TEST_CASE("page() draws, reads keys until q, and redraws on resize") {
  const std::string text = numbered(50);
  FakeTty tty({rows, 80}, {chr(' '), key(C::Resize), chr('q'), chr('j')});
  tui::page(tty, text);
  CHECK(tty.frames == 3);   // first screen, after space, after resize
  CHECK(tty.left() == 1);   // stopped at q
  CHECK(tty.written.find("lines 11-20 of 50") != std::string::npos);
  CHECK(tty.written.find("lines 11-25 of 50") != std::string::npos);  // 16 rows now
  FakeTty gone({rows, 80}, {chr(' ')});
  tui::page(gone, text);  // Eof after space: returns
  CHECK(gone.frames == 2);
}
