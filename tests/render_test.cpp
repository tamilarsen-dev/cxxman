// Display width, wrapping, colors and the highlighter (PLAN §4.2, §8.4, §8.5).
#include "render/highlight.hpp"
#include "render/layout.hpp"
#include "render/style.hpp"

#include <doctest/doctest.h>

#include <map>
#include <string>

using namespace cxxman::render;

TEST_CASE("display width counts columns, not bytes") {
  CHECK(display_width("sort") == 4);
  CHECK(display_width("✗ · …") == 5);  // symbols are one column
  CHECK(display_width("中文") == 4);         // East Asian wide
  CHECK(display_width("é") == 1);               // combining accent
  CHECK(display_width("\xff") == 1);                  // invalid byte: one replacement
}

TEST_CASE("wrap") {
  CHECK(wrap("one two three", 7) == std::vector<std::string>{"one two", "three"});
  CHECK(wrap("abcdefghij", 4) == std::vector<std::string>{"abcd", "efgh", "ij"});
  CHECK(wrap("", 10) == std::vector<std::string>{""});
}

TEST_CASE("flow keeps roles and hangs continuation lines") {
  Line prefix;
  prefix.add("  ! ", Role::Warn);
  auto lines = flow(prefix, words("aaa bbb ccc"), 11, 4);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0].width() == 11);
  CHECK(lines[1].spans().back().text == "    ccc");  // plain spans merge
  CHECK(lines[1].width() == 7);
}

TEST_CASE("put_right aligns or reports that it does not fit") {
  Line left, right;
  left.add("std::sort");
  right.add("C++98");
  CHECK(put_right(left, right, 20));
  CHECK(left.width() == 20);
  Line small;
  small.add("0123456789");
  CHECK_FALSE(put_right(small, right, 16));
  CHECK(small.width() == 10);
}

TEST_CASE("color depth from the environment") {
  auto depth = [](std::map<std::string, std::string> vars) {
    return detect_depth([&](const char* n) -> const char* {
      auto it = vars.find(n);
      return it == vars.end() ? nullptr : it->second.c_str();
    });
  };
  CHECK(depth({{"COLORTERM", "truecolor"}}) == Depth::True);
  CHECK(depth({{"WT_SESSION", "x"}}) == Depth::True);
  CHECK(depth({{"TERM_PROGRAM", "vscode"}}) == Depth::True);
  CHECK(depth({{"TERM_PROGRAM", "Apple_Terminal"}, {"TERM", "xterm-256color"}}) == Depth::Ansi256);
  CHECK(depth({{"TERM", "xterm"}}) == Depth::Ansi16);
  CHECK(nearest_256(0x000000) == 16);
  CHECK(nearest_256(0xffffff) == 231);
  CHECK(nearest_256(0x808080) == 244);
}

TEST_CASE("paint") {
  Block b(1);
  b[0].add("x", Role::Ub).add("  ");
  CHECK(paint(b, {}) == "x\n");  // trailing padding goes
  CHECK(paint(b, {Depth::True, Theme::Dark, false}) == "\x1b[38;2;224;108;117mx\x1b[0m\n");
  CHECK(paint(b, {Depth::Ansi256, Theme::Dark, false}) == "\x1b[38;5;168mx\x1b[0m\n");
  CHECK(paint(b, {Depth::Ansi16, Theme::Light, false}) == "\x1b[31mx\x1b[0m\n");
  CHECK(paint(b, {Depth::True, Theme::Mono, false}) == "\x1b[1mx\x1b[0m\n");
  CHECK(sym(Sym::Ub, true) == "x");
}

TEST_CASE("highlighter") {
  auto roles_of = [](std::string_view code) {
    auto roles = highlight(code);
    std::string out;
    for (auto r : roles) {
      switch (r) {
        case Role::Kw: out += 'k'; break;
        case Role::Str: out += 's'; break;
        case Role::Num: out += 'n'; break;
        case Role::Type: out += 't'; break;
        case Role::Note: out += 'c'; break;
        default: out += '.';
      }
    }
    return out;
  };
  CHECK(roles_of("int x = 1'000u;") == "kkk.....nnnnnn.");
  CHECK(roles_of("std::ranges::sort(v);") == "ttttttttttttttttt....");
  CHECK(roles_of("auto s = \"a\\\"b\"; // hi") == "kkkk.....ssssss..ccccc");
  CHECK(roles_of("R\"(a\"b)\" x") == "ssssssss..");
  CHECK(roles_of("#include <vector>\nx") == "kkkkkkkkkkkkkkkkk..");
  CHECK(roles_of("u8'c' /* a */ 1.5e-3") == "sssss.ccccccc.nnnnnn");
  CHECK(roles_of("\"open").size() == 5);  // never fails on broken input
}
