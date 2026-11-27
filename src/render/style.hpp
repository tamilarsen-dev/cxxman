// Themes, color depth and the final string with escape codes (PLAN §4.2).
#pragma once

#include "render/layout.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace cxxman::render {

enum class Depth : std::uint8_t { None, Ansi16, Ansi256, True };
enum class Theme : std::uint8_t { Dark, Light, Mono };

struct Style {
  Depth depth = Depth::None;  // None: no escape codes at all
  Theme theme = Theme::Dark;
  bool ascii = false;         // no Unicode symbols
};

using Getenv = std::function<const char*(const char*)>;

// Color depth from COLORTERM, TERM, WT_SESSION and TERM_PROGRAM (PLAN §4.2).
Depth detect_depth(const Getenv& env);

// xterm 256-color index nearest to an RGB color.
int nearest_256(std::uint32_t rgb);

// Symbols with an ASCII fallback.
enum class Sym : std::uint8_t { Ub, Warn, Perf, Dot, Ellipsis, Newer, Dash };
std::string_view sym(Sym s, bool ascii);

// One string, one '\n' per line; escape codes only when depth != None.
std::string paint(const Block& lines, const Style& style);

}  // namespace cxxman::render
