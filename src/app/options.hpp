// Command line, CXXMAN_OPTS / CXXMAN_STD and the config file, merged with the
// precedence flag > environment > config > default (PLAN §3.3).
#pragma once

#include "app/config.hpp"
#include "model/model.hpp"
#include "render/style.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cxxman::app {

enum class Mode : std::uint8_t { Auto, Always, Never };
enum class Source : std::uint8_t { Default, Config, Env, Flag };

// The settings a config file can hold, in display order.
inline constexpr std::array<std::string_view, 9> setting_keys{
    "theme", "color", "std", "strict", "width", "ascii", "hyperlinks", "pager", "interactive"};

struct Settings {
  render::Theme theme = render::Theme::Dark;
  Mode color = Mode::Auto;
  Std std = Std::Unknown;  // none
  bool strict = false;
  int width = 0;  // 0: the terminal's
  std::optional<bool> ascii;  // unset: automatic
  Mode hyperlinks = Mode::Auto;
  Mode pager = Mode::Auto;
  bool interactive = true;
  std::array<Source, setting_keys.size()> source{};  // per setting_keys entry

  Source source_of(std::string_view key) const;
  std::string value_of(std::string_view key) const;  // as --config shows it
};

enum class Action : std::uint8_t {
  Usage, Card, Search, Header, List, New, Coverage, Version, Help, Changelog,
  ConfigShow, ConfigPath, ConfigInit, Completions, Complete,
};

// What a card prints (PLAN §3.2): the whole card or one part of it. One at a time.
enum class Show : std::uint8_t { Card, Examples, Signatures, Brief, Include, Url, Web, Copy };

struct Invocation {
  Action action = Action::Usage;
  std::string name;  // Card, Search: the positional words joined by spaces
  std::string header;   // Header: the header; List: a filter
  std::string missing;  // Coverage: the header whose uncovered names to list
  Std new_std = Std::Unknown;  // New
  std::optional<Kind> kind;    // List filter
  Std since = Std::Unknown;    // List filter: since this standard or later
  Show show = Show::Card;      // Card
  bool support = false;        // Card: add the support line (--support)
  bool json = false;           // Card, Search, Header: JSON (docs/JSON.md)
  int copy = 1;                // Show::Copy: the example number, from 1
  std::string shell;           // Completions: bash, zsh, fish or powershell
  std::string prefix;          // Complete: what the shell has typed so far
  std::string modifier;        // the first card modifier given, for messages
  Settings settings;
  std::vector<std::string> warnings;  // printed, not fatal
  std::string error;                  // non-empty: usage error, exit 2
};

// Every option a user can type, short ones too, sorted (for --complete).
std::vector<std::string_view> option_names();

// `config` may be null (no file).
Invocation parse(std::span<const std::string> args, const render::Getenv& env,
                 const ConfigFile* config);

}  // namespace cxxman::app
