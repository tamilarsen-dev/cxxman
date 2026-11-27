#include "app/options.hpp"

#include "search/fuzzy.hpp"

#include <algorithm>
#include <charconv>

namespace cxxman::app {

namespace {

struct Flag {
  std::string_view name;
  std::string_view short_name;
  std::string_view key;    // the setting it sets; empty for actions
  bool takes_value = false;
  std::string_view fixed;  // the value a switch sets
  bool available = true;
};

constexpr Flag flags[] = {
    {"--color", "", "color", true, "", true},
    {"--theme", "", "theme", true, "", true},
    {"--std", "", "std", true, "", true},
    {"--strict", "", "strict", false, "true", true},
    {"--width", "", "width", true, "", true},
    {"--ascii", "", "ascii", false, "true", true},
    {"--no-hyperlinks", "", "hyperlinks", false, "never", true},
    {"--pager", "", "pager", true, "", true},
    {"--version", "", "", false, "", true},
    {"--help", "", "", false, "", true},
    {"--config", "", "", false, "", true},
    // Later milestones (PLAN §12.1): known, so typos get a useful hint.
    {"--search", "-s", "", false, "", false},
    {"--recipe", "-r", "", false, "", false},
    {"--compare", "-c", "", false, "", false},
    {"--header", "-H", "", false, "", false},
    {"--new", "", "", false, "", false},
    {"--list", "", "", false, "", false},
    {"--kind", "", "", false, "", false},
    {"--since", "", "", false, "", false},
    {"--coverage", "", "", false, "", false},
    {"--missing", "", "", false, "", false},
    {"--changelog", "", "", false, "", false},
    {"--completions", "", "", false, "", false},
    {"--examples", "-e", "", false, "", false},
    {"--sig", "", "", false, "", false},
    {"--brief", "-b", "", false, "", false},
    {"--include", "", "", false, "", false},
    {"--support", "", "", false, "", false},
    {"--copy", "", "", false, "", false},
    {"--web", "", "", false, "", false},
    {"--url", "", "", false, "", false},
    {"--json", "", "", false, "", false},
};

constexpr std::string_view later_commands[] = {"update", "check-updates", "self-update"};

std::optional<Mode> mode_of(std::string_view v) {
  if (v == "auto") return Mode::Auto;
  if (v == "always") return Mode::Always;
  if (v == "never") return Mode::Never;
  return std::nullopt;
}

std::optional<bool> bool_of(std::string_view v) {
  if (v == "true") return true;
  if (v == "false") return false;
  return std::nullopt;
}

std::optional<Std> std_of(std::string_view v) {
  if (v == "none") return Std::Unknown;
  if (v.starts_with("C++") || v.starts_with("c++")) v.remove_prefix(3);
  using P = std::pair<std::string_view, Std>;
  for (auto [text, value] : {P{"11", Std::Cpp11}, P{"14", Std::Cpp14}, P{"17", Std::Cpp17},
                             P{"20", Std::Cpp20}, P{"23", Std::Cpp23}, P{"26", Std::Cpp26}})
    if (v == text) return value;
  return std::nullopt;
}

std::string_view mode_name(Mode m) {
  switch (m) {
    case Mode::Always: return "always";
    case Mode::Never: return "never";
    case Mode::Auto: break;
  }
  return "auto";
}

std::size_t key_index(std::string_view key) {
  return static_cast<std::size_t>(std::ranges::find(setting_keys, key) - setting_keys.begin());
}

// Sets one setting from text. On a bad value, returns why and changes nothing.
std::string apply(Settings& s, std::string_view key, std::string_view v, Source src) {
  bool ok = true;
  if (key == "theme") {
    if (v == "dark") s.theme = render::Theme::Dark;
    else if (v == "light") s.theme = render::Theme::Light;
    else if (v == "mono") s.theme = render::Theme::Mono;
    else return "theme must be dark, light or mono, not '" + std::string(v) + "'";
  } else if (key == "color" || key == "hyperlinks" || key == "pager") {
    auto m = mode_of(v);
    if (!m) return std::string(key) + " must be auto, always or never, not '" + std::string(v) + "'";
    (key == "color" ? s.color : key == "pager" ? s.pager : s.hyperlinks) = *m;
  } else if (key == "std") {
    auto st = std_of(v);
    if (!st) return "std must be 11, 14, 17, 20, 23 or 26, not '" + std::string(v) + "'";
    s.std = *st;
  } else if (key == "width") {
    int w = 0;
    auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), w);
    if (ec != std::errc{} || end != v.data() + v.size() || w <= 0)
      return "width must be a number of columns, not '" + std::string(v) + "'";
    s.width = w;
  } else if (key == "strict" || key == "ascii" || key == "interactive") {
    auto b = bool_of(v);
    if (!b) return std::string(key) + " must be true or false, not '" + std::string(v) + "'";
    if (key == "strict") s.strict = *b;
    else if (key == "ascii") s.ascii = *b;
    else s.interactive = *b;
  } else {
    ok = false;
  }
  if (!ok) return "unknown setting '" + std::string(key) + "'";
  s.source[key_index(key)] = src;
  return {};
}

const Flag* find_flag(std::string_view name) {
  for (const auto& f : flags)
    if (f.name == name || (!f.short_name.empty() && f.short_name == name)) return &f;
  return nullptr;
}

std::string closest_flag(std::string_view name) {
  std::string_view best;
  int best_d = 3;
  for (const auto& f : flags) {
    const int d = search::edit_distance(name, f.name, 2);
    if (d < best_d) {
      best_d = d;
      best = f.name;
    }
  }
  return std::string(best);
}

// Splits CXXMAN_OPTS at spaces; "double quotes" group.
std::vector<std::string> tokens(std::string_view s) {
  std::vector<std::string> out;
  std::string cur;
  bool quoted = false, any = false;
  for (char c : s) {
    if (c == '"') {
      quoted = !quoted;
      any = true;
    } else if ((c == ' ' || c == '\t') && !quoted) {
      if (any) out.push_back(std::move(cur));
      cur.clear();
      any = false;
    } else {
      cur += c;
      any = true;
    }
  }
  if (any) out.push_back(std::move(cur));
  return out;
}

// Parses flags (and, unless options_only, actions and names). Returns false
// with inv.error set on the first problem.
bool parse_args(Invocation& inv, std::span<const std::string> args, Source src, bool options_only,
                std::string_view where, std::vector<std::string>& names) {
  auto fail = [&](std::string message) {
    inv.error = std::string(where) + message;
    return false;
  };
  bool only_names = false;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (only_names || arg.size() < 2 || arg[0] != '-') {
      if (options_only) return fail("'" + arg + "' is not an option");
      if (names.empty() && std::ranges::find(later_commands, arg) != std::end(later_commands))
        return fail("'" + arg + "' is not available yet");
      names.push_back(arg);
      continue;
    }
    if (arg == "--") {
      only_names = true;
      continue;
    }
    const auto eq = arg.find('=');
    const std::string name = arg.substr(0, eq);
    const Flag* f = find_flag(name);
    if (!f) {
      const auto hint = closest_flag(name);
      return fail("unknown option '" + name + "'" + (hint.empty() ? "" : " (did you mean " + hint + "?)"));
    }
    if (!f->available) return fail("'" + name + "' is not available yet");
    if (!f->key.empty()) {
      std::string value;
      if (f->takes_value) {
        if (eq != std::string::npos)
          value = arg.substr(eq + 1);
        else if (i + 1 < args.size())
          value = args[++i];
        else
          return fail(name + " needs a value");
      } else {
        if (eq != std::string::npos) return fail(name + " takes no value");
        value = f->fixed;
      }
      if (auto problem = apply(inv.settings, f->key, value, src); !problem.empty())
        return fail(name + ": " + problem);
      continue;
    }
    if (options_only) return fail("'" + name + "' is not an option you can set here");
    Action a = Action::Usage;
    if (name == "--version") {
      a = Action::Version;
    } else if (name == "--help") {
      a = Action::Help;
    } else {  // --config [init|path]
      a = Action::ConfigShow;
      if (i + 1 < args.size() && (args[i + 1] == "init" || args[i + 1] == "path"))
        a = args[++i] == "init" ? Action::ConfigInit : Action::ConfigPath;
    }
    if (inv.action == Action::Usage) inv.action = a;
  }
  return true;
}

}  // namespace

Source Settings::source_of(std::string_view key) const { return source[key_index(key)]; }

std::string Settings::value_of(std::string_view key) const {
  if (key == "theme") return theme == render::Theme::Light ? "light" : theme == render::Theme::Mono ? "mono" : "dark";
  if (key == "color") return std::string(mode_name(color));
  if (key == "hyperlinks") return std::string(mode_name(hyperlinks));
  if (key == "pager") return std::string(mode_name(pager));
  if (key == "std") return std == Std::Unknown ? "none" : std::string(to_string(std));
  if (key == "strict") return strict ? "true" : "false";
  if (key == "width") return width > 0 ? std::to_string(width) : "auto";
  if (key == "ascii") return ascii ? (*ascii ? "true" : "false") : "auto";
  if (key == "interactive") return interactive ? "true" : "false";
  return {};
}

Invocation parse(std::span<const std::string> args, const render::Getenv& env,
                 const ConfigFile* config) {
  Invocation inv;
  if (config) {
    for (const auto& e : config->entries) {
      const std::string at = config->file + ":" + std::to_string(e.line) + ": ";
      if (std::ranges::find(setting_keys, e.key) == setting_keys.end()) {
        inv.warnings.push_back(at + "unknown key '" + e.key + "'");
        continue;
      }
      if (auto problem = apply(inv.settings, e.key, e.value, Source::Config); !problem.empty())
        inv.warnings.push_back(at + problem + "; using the default");
    }
  }

  std::vector<std::string> names;
  if (const char* v = env("CXXMAN_STD"); v && *v) {
    if (auto problem = apply(inv.settings, "std", v, Source::Env); !problem.empty()) {
      inv.error = "CXXMAN_STD: " + problem;
      return inv;
    }
  }
  if (const char* v = env("CXXMAN_OPTS"); v && *v) {
    const auto opts = tokens(v);
    if (!parse_args(inv, opts, Source::Env, true, "CXXMAN_OPTS: ", names)) return inv;
  }
  if (!parse_args(inv, args, Source::Flag, false, "", names)) return inv;

  for (const auto& n : names) {
    if (!inv.name.empty()) inv.name += ' ';
    inv.name += n;
  }
  if (inv.action == Action::Usage && !inv.name.empty()) inv.action = Action::Card;
  return inv;
}

}  // namespace cxxman::app
