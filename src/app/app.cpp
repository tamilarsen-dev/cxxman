#include "app/app.hpp"

#include "app/config.hpp"
#include "app/options.hpp"
#include "render/card.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <system_error>

#ifndef CXXMAN_VERSION
#define CXXMAN_VERSION "0.0.0-unknown"
#endif

namespace cxxman::app {

namespace {

constexpr std::size_t min_width = 60, max_width = 100, fallback_width = 80;

constexpr std::string_view usage_text =
    "usage: cxxman [options] <name>\n"
    "try 'cxxman sort' or 'cxxman --help'\n";

constexpr std::string_view help_text =
    "cxxman - C++ standard library reference cards\n"
    "\n"
    "usage: cxxman [options] <name>      a card: sort, vector::erase, std::optional\n"
    "\n"
    "options:\n"
    "  --color auto|always|never        color output; auto honors NO_COLOR\n"
    "  --theme dark|light|mono\n"
    "  --width N                        60 to 100 columns\n"
    "  --ascii                          plain ASCII instead of Unicode symbols\n"
    "  --std 11..26                     your project's standard\n"
    "  --config [init|path]             show, create or locate the config file\n"
    "  --version, --help\n"
    "\n"
    "CXXMAN_OPTS holds default options, e.g. CXXMAN_OPTS=\"--theme light\".\n";

std::string_view source_name(Source s) {
  switch (s) {
    case Source::Config: return "config";
    case Source::Env: return "env";
    case Source::Flag: return "flag";
    case Source::Default: break;
  }
  return "default";
}

std::size_t width_for(const Settings& s, const Context& ctx) {
  std::size_t w = 0;
  if (s.width > 0) {
    w = static_cast<std::size_t>(s.width);
  } else if (ctx.term.out_tty && ctx.term.columns > 0) {
    w = ctx.term.columns;
  } else if (const char* c = ctx.env("COLUMNS"); c && *c) {
    std::string_view v = c;
    std::from_chars(v.data(), v.data() + v.size(), w);
  }
  if (w == 0) w = fallback_width;
  return std::clamp(w, min_width, max_width);
}

bool color_on(const Settings& s, const Context& ctx) {
  switch (s.color) {
    case Mode::Always: return true;
    case Mode::Never: return false;
    case Mode::Auto: break;
  }
  auto set = [&](const char* name) {
    const char* v = ctx.env(name);
    return v && *v;
  };
  if (set("NO_COLOR")) return false;
  if (const char* force = ctx.env("CLICOLOR_FORCE"); force && *force && std::string_view(force) != "0")
    return true;
  if (!ctx.term.out_tty || !ctx.term.vt) return false;
  const char* term = ctx.env("TERM");
  return !(term && std::string_view(term) == "dumb");
}

std::string show_config(const Settings& s, const Context& ctx) {
  std::string out = "config: ";
  if (ctx.config_file.empty()) {
    out += "(no config directory)\n";
  } else {
    out += ctx.config_file.string();
    std::error_code ec;
    if (!std::filesystem::exists(ctx.config_file, ec)) out += " (not found; cxxman --config init creates it)";
    out += '\n';
  }
  for (auto key : setting_keys) {
    std::string line = "  " + std::string(key);
    line.resize(15, ' ');
    line += s.value_of(key);
    line.resize(std::max<std::size_t>(line.size(), 26), ' ');
    line += source_name(s.source_of(key));
    out += line + '\n';
  }
  return out;
}

int init_config(const Context& ctx, Output& io) {
  if (ctx.config_file.empty()) {
    io.err += "cxxman: no config directory (set APPDATA or HOME)\n";
    return 2;
  }
  std::error_code ec;
  if (std::filesystem::exists(ctx.config_file, ec)) {
    io.err += "cxxman: " + ctx.config_file.string() + " already exists; edit it or remove it first\n";
    return 2;
  }
  std::filesystem::create_directories(ctx.config_file.parent_path(), ec);
  std::ofstream f(ctx.config_file, std::ios::binary);
  const auto text = default_config();
  f.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (!f) {
    io.err += "cxxman: cannot write " + ctx.config_file.string() + "\n";
    return 3;
  }
  io.out += "wrote " + ctx.config_file.string() + "\n";
  return 0;
}

int show_name(const Invocation& inv, const Context& ctx, Output& io) {
  const Settings& s = inv.settings;
  const render::Options o{width_for(s, ctx), s.ascii.value_or(!ctx.term.utf8)};
  render::Style style;
  style.theme = s.theme;
  style.ascii = o.ascii;
  if (color_on(s, ctx)) style.depth = render::detect_depth(ctx.env);
  const render::Style plain{render::Depth::None, s.theme, o.ascii};

  const Resolution r = resolve(ctx.catalog, inv.name);
  switch (r.what) {
    case Resolution::What::Card:
      io.out += render::paint(
          render::card(ctx.catalog, ctx.catalog.entries[static_cast<std::size_t>(r.entry)], r.also, o),
          style);
      return 0;
    case Resolution::What::Index:
      io.out += render::paint(render::index_only(ctx.catalog, r.rows, r.also, o), style);
      return 0;
    case Resolution::What::Ambiguous:
      io.err += render::paint(render::ambiguous(inv.name, r.also, o), plain);
      return 2;
    case Resolution::What::Missing:
      break;
  }
  io.err += render::paint(render::not_found(inv.name, r.suggestions, o), plain);
  return 1;
}

}  // namespace

int run(std::span<const std::string> args, const Context& ctx, Output& io) {
  ConfigFile config;
  const ConfigFile* config_ptr = nullptr;
  if (!ctx.config_file.empty()) {
    std::ifstream f(ctx.config_file, std::ios::binary);
    if (f) {
      const std::string text{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
      config = parse_config(text, ctx.config_file.string());
      config_ptr = &config;
      for (const auto& p : config.problems) io.err += "cxxman: " + p + "\n";
    }
  }

  const Invocation inv = parse(args, ctx.env, config_ptr);
  for (const auto& w : inv.warnings) io.err += "cxxman: " + w + "\n";
  if (!inv.error.empty()) {
    io.err += "cxxman: " + inv.error + "\n";
    return 2;
  }

  switch (inv.action) {
    case Action::Version: io.out += "cxxman " CXXMAN_VERSION "\n"; return 0;
    case Action::Help: io.out += help_text; return 0;
    case Action::ConfigPath:
      io.out += (ctx.config_file.empty() ? std::string("(no config directory)") : ctx.config_file.string()) + "\n";
      return 0;
    case Action::ConfigShow: io.out += show_config(inv.settings, ctx); return 0;
    case Action::ConfigInit: return init_config(ctx, io);
    case Action::Card: return show_name(inv, ctx, io);
    case Action::Usage: break;
  }
  io.err += usage_text;
  return 2;
}

}  // namespace cxxman::app
