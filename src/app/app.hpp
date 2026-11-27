// What `cxxman` does with its arguments, without touching the real terminal:
// main() supplies the environment and writes the output, tests capture it.
#pragma once

#include "model/lookup.hpp"
#include "render/style.hpp"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>

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

struct Output {
  std::string out, err;
};

// Exit codes (PLAN §3.5): 0 ok, 1 not found, 2 ambiguous or usage, 3 internal.
int run(std::span<const std::string> args, const Context& ctx, Output& io);

}  // namespace cxxman::app
