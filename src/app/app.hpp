// What `cxxman` does with its arguments, without touching the real terminal:
// main() supplies the environment and writes the output, tests capture it.
#pragma once

#include "model/lookup.hpp"
#include "render/style.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace cxxman::app {

struct Terminal {
  bool out_tty = false;
  std::size_t columns = 0;  // 0: unknown
  bool vt = true;           // escape codes are understood
  bool utf8 = true;         // the console shows UTF-8
};

struct Context {
  render::Getenv env;
  Terminal term;
  Catalog catalog;
  std::filesystem::path config_file;  // empty: none
};

// When main() shows `out` in the built-in pager (PLAN §4.8): Auto when it is
// taller than the terminal.
enum class Paging : std::uint8_t { Off, Auto, Always };

// What run() printed, and what main() should do for it afterwards; run()
// itself never opens a browser, touches the clipboard or pages, so tests can
// see what it asked for.
struct Output {
  std::string out, err;
  std::string open_url;   // --web: a page that passed allowed_url()
  std::string clipboard;  // --copy: text for the clipboard
  std::string copied;     // --copy: "example 1 (3 lines)", for the message
  Paging page = Paging::Off;  // stdout is a terminal and --pager allows it
};

// Exit codes (PLAN §3.5): 0 ok, 1 not found, 2 ambiguous or usage, 3 internal.
int run(std::span<const std::string> args, const Context& ctx, Output& io);

// --web opens only cppreference pages, and only URLs made of the characters
// its paths use.
bool allowed_url(std::string_view url);

}  // namespace cxxman::app
