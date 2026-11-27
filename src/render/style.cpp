#include "render/style.hpp"

#include <array>
#include <cstdlib>
#include <string>

namespace cxxman::render {

namespace {

constexpr std::uint32_t no_color = 0xFFFFFFFF;

struct Attr {
  std::uint32_t fg = no_color;  // truecolor value
  int ansi = -1;                // 16-color SGR code (30-37, 90-97)
  bool bold = false, dim = false, italic = false, underline = false, reverse = false;
};

constexpr std::size_t role_count = static_cast<std::size_t>(Role::Accent) + 1;
using Palette = std::array<Attr, role_count>;

// Order follows Role. The 16-color codes are the same for dark and light.
constexpr Palette dark{{
    /* Plain  */ {},
    /* Title  */ {.bold = true},
    /* Header */ {.fg = 0x56b6c2, .ansi = 36},
    /* Badge  */ {.fg = 0x7f848e, .ansi = 90},
    /* Sig    */ {.fg = 0x98c379, .ansi = 32},
    /* Note   */ {.fg = 0x7f848e, .ansi = 90, .italic = true},
    /* Kw     */ {.fg = 0xc678dd, .ansi = 35},
    /* Str    */ {.fg = 0x98c379, .ansi = 32},
    /* Num    */ {.fg = 0xd19a66, .ansi = 33},
    /* Type   */ {.fg = 0x56b6c2, .ansi = 36},
    /* Ub     */ {.fg = 0xe06c75, .ansi = 31},
    /* Warn   */ {.fg = 0xe5c07b, .ansi = 33},
    /* Perf   */ {.fg = 0x61afef, .ansi = 34},
    /* Link   */ {.bold = true, .underline = true},
    /* Newer  */ {.fg = 0x7f848e, .ansi = 90},
    /* Accent */ {.fg = 0x61afef, .ansi = 34},
}};

constexpr Palette light{{
    /* Plain  */ {},
    /* Title  */ {.bold = true},
    /* Header */ {.fg = 0x0e7490, .ansi = 36},
    /* Badge  */ {.fg = 0x6b7280, .ansi = 90},
    /* Sig    */ {.fg = 0x15803d, .ansi = 32},
    /* Note   */ {.fg = 0x6b7280, .ansi = 90, .italic = true},
    /* Kw     */ {.fg = 0x7c3aed, .ansi = 35},
    /* Str    */ {.fg = 0x15803d, .ansi = 32},
    /* Num    */ {.fg = 0xb45309, .ansi = 33},
    /* Type   */ {.fg = 0x0e7490, .ansi = 36},
    /* Ub     */ {.fg = 0xb91c1c, .ansi = 31},
    /* Warn   */ {.fg = 0xa16207, .ansi = 33},
    /* Perf   */ {.fg = 0x1d4ed8, .ansi = 34},
    /* Link   */ {.bold = true, .underline = true},
    /* Newer  */ {.fg = 0x6b7280, .ansi = 90},
    /* Accent */ {.fg = 0x1d4ed8, .ansi = 34},
}};

// Bold, dim and underline only.
constexpr Palette mono{{
    /* Plain  */ {},
    /* Title  */ {.bold = true},
    /* Header */ {.underline = true},
    /* Badge  */ {.dim = true},
    /* Sig    */ {},
    /* Note   */ {.dim = true},
    /* Kw     */ {.bold = true},
    /* Str    */ {},
    /* Num    */ {},
    /* Type   */ {},
    /* Ub     */ {.bold = true},
    /* Warn   */ {.bold = true},
    /* Perf   */ {},
    /* Link   */ {.bold = true, .underline = true},
    /* Newer  */ {.dim = true},
    /* Accent */ {.reverse = true},
}};

const Palette& palette(Theme t) {
  switch (t) {
    case Theme::Light: return light;
    case Theme::Mono: return mono;
    case Theme::Dark: break;
  }
  return dark;
}

std::string sgr(const Attr& a, Depth depth, Theme theme) {
  std::string codes;
  auto add = [&](std::string_view c) {
    if (!codes.empty()) codes += ';';
    codes += c;
  };
  if (a.bold) add("1");
  if (a.dim) add("2");
  if (a.italic) add("3");
  if (a.underline) add("4");
  if (a.reverse) add("7");
  if (theme != Theme::Mono && a.fg != no_color) {
    const unsigned r = (a.fg >> 16) & 0xFF, g = (a.fg >> 8) & 0xFF, b = a.fg & 0xFF;
    if (depth == Depth::True)
      add("38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b));
    else if (depth == Depth::Ansi256)
      add("38;5;" + std::to_string(nearest_256(a.fg)));
    else if (a.ansi >= 0)
      add(std::to_string(a.ansi));
  }
  return codes.empty() ? std::string{} : "\x1b[" + codes + "m";
}

bool contains(std::string_view s, std::string_view part) { return s.find(part) != std::string_view::npos; }

}  // namespace

Depth detect_depth(const Getenv& env) {
  auto get = [&](const char* name) -> std::string_view {
    const char* v = env(name);
    return v ? v : "";
  };
  const auto colorterm = get("COLORTERM");
  if (colorterm == "truecolor" || colorterm == "24bit") return Depth::True;
  if (!get("WT_SESSION").empty()) return Depth::True;
  const auto program = get("TERM_PROGRAM");
  if (program == "iTerm.app" || program == "WezTerm" || program == "vscode" || program == "ghostty")
    return Depth::True;
  if (contains(get("TERM"), "256color")) return Depth::Ansi256;
  return Depth::Ansi16;
}

int nearest_256(std::uint32_t rgb) {
  const int c[3] = {static_cast<int>((rgb >> 16) & 0xFF), static_cast<int>((rgb >> 8) & 0xFF),
                    static_cast<int>(rgb & 0xFF)};
  constexpr int levels[6] = {0, 95, 135, 175, 215, 255};
  auto level = [&](int v) {
    int best = 0;
    for (int i = 1; i < 6; ++i)
      if (std::abs(levels[i] - v) < std::abs(levels[best] - v)) best = i;
    return best;
  };
  auto dist = [&](int r, int g, int b) {
    return (r - c[0]) * (r - c[0]) + (g - c[1]) * (g - c[1]) + (b - c[2]) * (b - c[2]);
  };
  const int ri = level(c[0]), gi = level(c[1]), bi = level(c[2]);
  const int cube = 16 + 36 * ri + 6 * gi + bi;
  const int cube_d = dist(levels[ri], levels[gi], levels[bi]);
  const int avg = (c[0] + c[1] + c[2]) / 3;
  const int gi_gray = avg < 8 ? 0 : avg > 238 ? 23 : (avg - 8 + 5) / 10;
  const int gray = 8 + 10 * gi_gray;
  return dist(gray, gray, gray) < cube_d ? 232 + gi_gray : cube;
}

std::string_view sym(Sym s, bool ascii) {
  switch (s) {
    case Sym::Ub: return ascii ? "x" : "✗";
    case Sym::Warn: return "!";
    case Sym::Perf: return "~";
    case Sym::Dot: return ascii ? "|" : "·";
    case Sym::Ellipsis: return ascii ? "..." : "…";
    case Sym::Newer: return ascii ? "!" : "⚑";
    case Sym::Dash: return ascii ? "-" : "—";
  }
  return "?";
}

std::string paint(const Block& lines, const Style& style) {
  std::string out;
  const Palette& p = palette(style.theme);
  for (const auto& line : lines) {
    std::string text;
    for (const auto& span : line.spans()) {
      const std::string on = style.depth == Depth::None
                                 ? std::string{}
                                 : sgr(p[static_cast<std::size_t>(span.role)], style.depth, style.theme);
      if (on.empty()) {
        text += span.text;
      } else {
        text += on;
        text += span.text;
        text += "\x1b[0m";
      }
    }
    // Padding before a right-aligned item can leave trailing spaces.
    while (!text.empty() && text.back() == ' ') text.pop_back();
    out += text;
    out += '\n';
  }
  return out;
}

}  // namespace cxxman::render
