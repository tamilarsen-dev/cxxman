// Display width, wrapping, colors and the highlighter (PLAN §4.2, §8.4, §8.5).
#include "render/card.hpp"
#include "render/highlight.hpp"
#include "render/layout.hpp"
#include "render/style.hpp"
#include "sys/clipboard.hpp"

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

TEST_CASE("truncate counts columns and adds an ellipsis only when it cuts") {
  CHECK(truncate("short", 10, "…") == "short");
  CHECK(truncate("exactly10!", 10, "…") == "exactly10!");
  CHECK(truncate("one two three", 8, "…") == "one two…");  // no space before the …
  CHECK(truncate("one two three", 8, "...") == "one t...");
  CHECK(truncate("中文中文", 5, "…") == "中文…");          // wide characters
  CHECK(truncate("abc", 0, "…") == "…");
}

TEST_CASE("group_digits") {
  CHECK(group_digits(0) == "0");
  CHECK(group_digits(999) == "999");
  CHECK(group_digits(3225) == "3,225");
  CHECK(group_digits(1234567) == "1,234,567");
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

TEST_CASE("OSC 8 links: which terminals, and how a link is painted (PLAN 4.7)") {
  auto links = [](std::map<std::string, std::string> vars) {
    return detect_hyperlinks([&](const char* n) -> const char* {
      auto it = vars.find(n);
      return it == vars.end() ? nullptr : it->second.c_str();
    });
  };
  CHECK(links({{"WT_SESSION", "x"}}));
  CHECK(links({{"TERM_PROGRAM", "WezTerm"}}));
  CHECK(links({{"TERM_PROGRAM", "ghostty"}}));
  CHECK(links({{"TERM", "xterm-kitty"}}));
  CHECK(links({{"VTE_VERSION", "6003"}}));
  CHECK_FALSE(links({{"VTE_VERSION", "4802"}}));
  CHECK_FALSE(links({{"VTE_VERSION", "60x"}}));
  CHECK_FALSE(links({{"TERM_PROGRAM", "Apple_Terminal"}}));
  CHECK_FALSE(links({}));

  Block b(1);
  b[0].add("sort", Role::Link, "https://e.x/sort").add(" ").add("<a>", Role::Header, "https://e.x/a");
  const Style on{Depth::Ansi16, Theme::Dark, false, true};
  CHECK(paint(b, on) ==
        "\x1b]8;;https://e.x/sort\x1b\\\x1b[1;4msort\x1b[0m\x1b]8;;\x1b\\ "
        "\x1b]8;;https://e.x/a\x1b\\\x1b[36m<a>\x1b[0m\x1b]8;;\x1b\\\n");
  CHECK(paint(b, {Depth::Ansi16, Theme::Dark, false, false}) == "\x1b[1;4msort\x1b[0m \x1b[36m<a>\x1b[0m\n");
  CHECK(paint(b, {Depth::None, Theme::Dark, false, true}) == "sort <a>\n");  // no color, no links

  // Spans with different targets never merge.
  Line l;
  l.add("a", Role::Link, "u1").add("b", Role::Link, "u2").add("c", Role::Link, "u2");
  REQUIRE(l.spans().size() == 2);
  CHECK(l.spans()[1].text == "bc");
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

TEST_CASE("std_of_badge reads the standard a badge names (--std marking)") {
  CHECK(cxxman::render::std_of_badge("constexpr C++20") == cxxman::Std::Cpp20);
  CHECK(cxxman::render::std_of_badge("parallel C++17") == cxxman::Std::Cpp17);
  CHECK(cxxman::render::std_of_badge("C++11") == cxxman::Std::Cpp11);
  CHECK(cxxman::render::std_of_badge("freestanding") == cxxman::Std::Unknown);
  CHECK(cxxman::render::std_of_badge("C++2c") == cxxman::Std::Unknown);
}

TEST_CASE("base64 and the OSC 52 clipboard sequence (PLAN 4.9)") {
  using cxxman::sys::base64;
  CHECK(base64("") == "");
  CHECK(base64("f") == "Zg==");
  CHECK(base64("fo") == "Zm8=");
  CHECK(base64("foo") == "Zm9v");
  CHECK(base64("foobar") == "Zm9vYmFy");
  CHECK(base64("\xff\xfe") == "//4=");
  CHECK(cxxman::sys::osc52("hi") == "\x1b]52;c;aGk=\x07");
}
