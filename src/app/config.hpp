// The optional config file (PLAN §7.5): a flat `key = value` subset of TOML,
// read by hand. The only runtime parsing cxxman does.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cxxman::app {

struct ConfigEntry {
  std::string key, value;  // value unquoted
  int line = 0;
};

struct ConfigFile {
  std::string file;
  std::vector<ConfigEntry> entries;
  std::vector<std::string> problems;  // "config.toml:3: ..."; never fatal
};

// Strings may be quoted ("dark") or bare (dark); ints and bools are bare.
ConfigFile parse_config(std::string_view text, std::string_view file);

// What `cxxman --config init` writes: every key, commented out.
std::string_view default_config();

}  // namespace cxxman::app
