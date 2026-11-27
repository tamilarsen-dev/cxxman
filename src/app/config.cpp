#include "app/config.hpp"

#include <algorithm>

namespace cxxman::app {

namespace {

std::string_view trim(std::string_view s) {
  const auto first = s.find_first_not_of(" \t\r");
  if (first == std::string_view::npos) return {};
  return s.substr(first, s.find_last_not_of(" \t\r") - first + 1);
}

bool key_char(char c) { return (c >= 'a' && c <= 'z') || c == '_'; }

}  // namespace

ConfigFile parse_config(std::string_view text, std::string_view file) {
  ConfigFile out;
  out.file = file;
  int line_no = 0;
  std::size_t pos = 0;
  while (pos <= text.size()) {
    auto end = text.find('\n', pos);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(pos, end - pos);
    pos = end + 1;
    ++line_no;
    auto problem = [&](std::string_view what) {
      out.problems.push_back(std::string(file) + ":" + std::to_string(line_no) + ": " +
                             std::string(what));
    };

    line = trim(line);
    if (line.empty() || line.front() == '#') continue;
    const auto eq = line.find('=');
    if (eq == std::string_view::npos) {
      problem("expected key = value");
      continue;
    }
    const std::string_view key = trim(line.substr(0, eq));
    if (key.empty() || !std::ranges::all_of(key, key_char)) {
      problem("bad key '" + std::string(key) + "'");
      continue;
    }
    std::string_view rest = trim(line.substr(eq + 1));
    std::string value;
    if (!rest.empty() && rest.front() == '"') {
      std::size_t i = 1;
      bool closed = false;
      for (; i < rest.size(); ++i) {
        if (rest[i] == '\\' && i + 1 < rest.size()) {
          value += rest[++i];
        } else if (rest[i] == '"') {
          closed = true;
          break;
        } else {
          value += rest[i];
        }
      }
      rest = trim(rest.substr(closed ? i + 1 : rest.size()));
      if (!closed || (!rest.empty() && rest.front() != '#')) {
        problem("bad string value for '" + std::string(key) + "'");
        continue;
      }
    } else {
      const auto hash = rest.find('#');
      value = trim(rest.substr(0, hash));
      if (value.empty() || value.find_first_of(" \t") != std::string::npos) {
        problem("bad value for '" + std::string(key) + "'");
        continue;
      }
    }
    out.entries.push_back({std::string(key), std::move(value), line_no});
  }
  return out;
}

std::string_view default_config() {
  return R"(# cxxman configuration. Flags and the CXXMAN_OPTS variable override it.
# Remove the # in front of a line to use it.

# theme = "dark"        # dark, light or mono
# color = "auto"        # auto, always or never; auto honors NO_COLOR
# std = 17              # your project's standard: 11, 14, 17, 20, 23 or 26
# strict = false        # with std: hide newer items instead of marking them
# width = 100           # 60 to 100 columns; default: the terminal width
# ascii = false         # plain ASCII instead of Unicode symbols
# hyperlinks = "auto"   # auto, always or never
# pager = "auto"        # auto, always or never
# interactive = true    # a bare `cxxman` in a terminal opens the finder
)";
}

}  // namespace cxxman::app
