// The interactive finder (PLAN §4.6, §10): keys in, frames out, through a
// fake terminal. Frame snapshots are tests/golden/finder-<case>.txt;
// CXXMAN_UPDATE_GOLDEN=1 rewrites them.
#include "tui/finder.hpp"

#include "fake_tty.hpp"
#include "render/layout.hpp"
#include "sys/paths.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using namespace cxxman;
using term::chr;
using term::ctrl;
using term::key;
using C = term::Key::Code;
using A = tui::Finder::Action;

namespace {

const Catalog& catalog() {
  static const Catalog cat = Catalog::builtin();
  return cat;
}

const tui::FinderModel& model() {
  static const tui::FinderModel m(catalog());
  return m;
}

tui::Finder finder(bool ascii = false) {
  tui::FinderSettings s;
  s.ascii = ascii;
  s.style.ascii = ascii;
  return tui::Finder(catalog(), model(), s);
}

void type(tui::Finder& f, std::string_view text, term::Size s = {24, 100}) {
  for (char c : text) f.handle(chr(static_cast<unsigned char>(c)), s);
}

std::string selected(const tui::Finder& f) { return f.selected() ? std::string(f.selected()->name) : "-"; }

// The frame as text: one line per row, then a line of '~' to mark its end.
std::string snapshot(tui::Finder& f, term::Size s) {
  std::string out;
  for (const auto& row : f.frame(s)) out += row + "\n";
  return out + "~\n";
}

void check_golden(const std::string& name, const std::string& actual) {
  const auto path = std::filesystem::path(CXXMAN_GOLDEN_DIR) / ("finder-" + name + ".txt");
  const char* update = sys::getenv("CXXMAN_UPDATE_GOLDEN");
  if (update && std::string_view(update) == "1") {
    std::ofstream(path, std::ios::binary | std::ios::trunc) << actual;
    return;
  }
  std::ifstream in(path, std::ios::binary);
  std::string expected{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  std::erase(expected, '\r');
  INFO("golden: ", path.string(), " (CXXMAN_UPDATE_GOLDEN=1 rewrites it)");
  CHECK(expected == actual);
}

// Every row fits the screen, and there are never more rows than the screen.
void check_fits(tui::Finder& f, term::Size s) {
  const auto rows = f.frame(s);
  CHECK(rows.size() <= s.rows);
  for (const auto& r : rows) {
    INFO("row: ", r);
    CHECK(render::display_width(r) <= s.cols);
  }
}

}  // namespace

TEST_CASE("finder frames: side, stacked, list, ascii, headers, no match") {
  auto f = finder();
  type(f, "sort");
  check_golden("side-sort", snapshot(f, {16, 100}));
  check_golden("stacked-sort", snapshot(f, {24, 80}));
  check_golden("list-sort", snapshot(f, {10, 50}));
  auto a = finder(true);
  type(a, "<alg");
  check_golden("side-headers-ascii", snapshot(a, {14, 100}));
  auto n = finder();
  type(n, "zzzzqq");
  check_golden("nothing", snapshot(n, {10, 80}));
  auto e = finder();
  type(e, "unique");
  e.handle(ctrl('e'), {20, 100});
  e.handle(ctrl('s'), {20, 100});
  e.handle(ctrl('s'), {20, 100});
  check_golden("examples-std14", snapshot(e, {20, 100}));
}

TEST_CASE("finder frames fit every size") {
  for (term::Size s : {term::Size{24, 100}, {24, 140}, {30, 99}, {20, 60}, {12, 59}, {5, 40}, {3, 30}, {1, 20}}) {
    INFO("size ", s.rows, "x", s.cols);
    for (std::string_view q : {"", "sort", "<", "#", "vector", "remove duplicates"}) {
      auto f = finder();
      type(f, q, s);
      check_fits(f, s);
      f.handle(key(C::Tab), s);
      check_fits(f, s);
    }
  }
}

TEST_CASE("finder query editing") {
  auto f = finder();
  CHECK(f.match_count() == model().size());
  type(f, "vectt");
  f.handle(key(C::Backspace), {24, 100});
  CHECK(f.query() == "vect");
  CHECK(selected(f) == "vector");
  f.handle(chr(U'é'), {24, 100});
  f.handle(key(C::Backspace), {24, 100});
  CHECK(f.query() == "vect");
  f.handle(ctrl('u'), {24, 100});
  CHECK(f.query().empty());
  f.handle(key(C::Backspace), {24, 100});  // nothing to delete
  CHECK(f.query().empty());
  CHECK(f.cursor_column() == 11);  // after " cxxman ❯ "
  type(f, "ab");
  CHECK(f.cursor_column() == 13);
}

TEST_CASE("finder selection moves and scrolls") {
  auto f = finder();
  const term::Size s{12, 100};  // 8 list rows
  type(f, "s", s);
  const std::string first = selected(f);
  f.handle(key(C::Down), s);
  f.handle(ctrl('n'), s);
  const std::string third = selected(f);
  CHECK(third != first);
  f.handle(key(C::Up), s);
  f.handle(ctrl('p'), s);
  CHECK(selected(f) == first);
  f.handle(key(C::Up), s);  // stays at the top
  CHECK(selected(f) == first);
  f.handle(key(C::PageDown), s);
  f.handle(key(C::PageDown), s);
  // The selection is always on screen: the marked row is in the frame.
  const auto rows = f.frame(s);
  bool marked = false;
  for (const auto& r : rows) marked = marked || r.find("│▌" + selected(f)) != std::string::npos;
  CHECK(marked);
  f.handle(key(C::End), s);
  const std::string last = selected(f);
  f.handle(key(C::Down), s);
  CHECK(selected(f) == last);
  f.handle(key(C::Home), s);
  CHECK(selected(f) == first);
  type(f, "o", s);  // a new query starts at the top
  CHECK(f.query() == "so");
}

TEST_CASE("finder keys that ask for actions") {
  auto f = finder();
  const term::Size s{24, 100};
  CHECK(f.handle(key(C::Esc), s) == A::Quit);
  CHECK(f.handle(ctrl('c'), s) == A::Quit);
  type(f, "sort");
  CHECK(f.handle(key(C::Enter), s) == A::Open);
  CHECK(f.handle(ctrl('o'), s) == A::Print);
  CHECK(f.handle(ctrl('y'), s) == A::Copy);
  CHECK(f.handle(ctrl('w'), s) == A::Web);
  // Ctrl-E and Ctrl-S stay inside the finder.
  CHECK(f.handle(ctrl('e'), s) == A::None);
  CHECK(f.examples());
  for (int i = 0; i < 3; ++i) CHECK(f.handle(ctrl('s'), s) == A::None);
  CHECK(f.standard() == Std::Cpp17);
  for (int i = 0; i < 4; ++i) f.handle(ctrl('s'), s);
  CHECK(f.standard() == Std::Unknown);  // 20, 23, 26, then none again
  // Nothing selected: Enter and the others do nothing.
  type(f, "zzzzqq");
  CHECK(f.match_count() == 0);
  CHECK(f.handle(key(C::Enter), s) == A::None);
  CHECK(f.handle(ctrl('y'), s) == A::None);
}

TEST_CASE("finder Tab turns the preview off and on") {
  auto f = finder();
  type(f, "sort");
  const term::Size wide{16, 100}, narrow{16, 50};
  CHECK(f.frame(wide)[1].starts_with("╭"));  // side by side
  f.handle(key(C::Tab), wide);
  CHECK(!f.frame(wide)[1].starts_with("╭"));  // list only
  f.handle(key(C::Tab), wide);
  CHECK(f.frame(wide)[1].starts_with("╭"));
  auto g = finder();
  type(g, "sort");
  const auto off = g.frame(narrow);  // under 60: no preview
  g.handle(key(C::Tab), narrow);
  CHECK(g.frame(narrow) != off);  // stacked preview after Tab
}

TEST_CASE("run() draws only what changed and returns the first action") {
  auto f = finder();
  test::FakeTty tty({16, 100}, {chr('s'), chr('o'), key(C::Resize), chr('r'), chr('t'), key(C::Enter), chr('x')});
  CHECK(tui::run(tty, f) == A::Open);
  CHECK(tty.left() == 1);
  CHECK(tty.frames == 6);
  CHECK(f.query() == "sort");
  CHECK(selected(f) == "sort");
  // Resize clears the screen once more (the first frame clears it too).
  std::size_t clears = 0;
  for (auto p = tty.written.find("\x1b[2J"); p != std::string::npos; p = tty.written.find("\x1b[2J", p + 1)) ++clears;
  CHECK(clears == 2);
  // The input going away quits.
  test::FakeTty gone({16, 100}, {});
  CHECK(tui::run(gone, f) == A::Quit);
}

TEST_CASE("session: Enter pages and q returns, Ctrl-Y and Ctrl-W report, Ctrl-O prints") {
  std::vector<std::string> copied, opened;
  tui::Effects fx;
  fx.copy = [&](const std::string& t) {
    copied.push_back(t);
    return tui::Effects::Copied::Clipboard;
  };
  fx.open = [&](const std::string& u) {
    opened.push_back(u);
    return true;
  };
  auto f = finder();
  test::FakeTty tty({16, 100}, {chr('s'), chr('o'), chr('r'), chr('t'), key(C::Enter), chr('q'), ctrl('y'),
                                ctrl('w'), ctrl('o')});
  const std::string printed = tui::session(tty, f, fx);
  CHECK(tty.left() == 0);
  CHECK(printed.starts_with("std::sort  <algorithm>"));
  CHECK(printed.find("Sort a range in ascending order.") != std::string::npos);
  REQUIRE(copied.size() == 1);
  CHECK(copied[0].starts_with("std::sort(v.begin(), v.end());"));
  CHECK(opened == std::vector<std::string>{"https://en.cppreference.com/cpp/algorithm/sort"});
  // The pager showed the card; the notices showed in the help line.
  CHECK(tty.written.find("lines 1-") != std::string::npos);
  CHECK(tty.written.find("copied example 1 (") != std::string::npos);
  CHECK(tty.written.find("opened https://en.cppreference.com/cpp/algorithm/sort") != std::string::npos);
}

TEST_CASE("session: failures and things without examples or pages") {
  tui::Effects fx;
  fx.copy = [](const std::string&) { return tui::Effects::Copied::No; };
  fx.open = [](const std::string&) { return false; };
  auto f = finder();
  test::FakeTty tty({16, 100}, {chr('s'), chr('o'), chr('r'), chr('t'), ctrl('y'), ctrl('w'), ctrl('u'), chr('#'),
                                ctrl('y'), ctrl('w'), key(C::Esc)});
  CHECK(tui::session(tty, f, fx).empty());  // Esc prints nothing
  CHECK(tty.written.find("no clipboard tool found") != std::string::npos);
  CHECK(tty.written.find("could not open a browser; the page is https://") != std::string::npos);
  CHECK(tty.written.find("no example to copy") != std::string::npos);    // a recipe
  CHECK(tty.written.find("no web page for this one") != std::string::npos);
  // Eof ends it too.
  test::FakeTty gone({16, 100}, {});
  CHECK(tui::session(gone, f, fx).empty());
}

TEST_CASE("finder text and url follow the selection and Ctrl-E") {
  auto f = finder();
  type(f, "istringstream");
  CHECK(f.url().starts_with("https://en.cppreference.com/cpp/"));
  CHECK(f.example().first.empty());  // an index name has no examples
  f.handle(ctrl('u'), {24, 100});
  type(f, "unique");
  const std::string full = f.text(80);
  f.handle(ctrl('e'), {24, 100});
  const std::string ex = f.text(80);
  CHECK(ex != full);
  CHECK(ex.find("// drop all dups") != std::string::npos);
  f.handle(ctrl('u'), {24, 100});
  type(f, "<algorithm");
  CHECK(f.url() == "https://en.cppreference.com/cpp/header/algorithm");
}
