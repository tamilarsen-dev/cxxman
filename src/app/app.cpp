#include "app/app.hpp"

#include "app/completions.hpp"
#include "app/config.hpp"
#include "app/options.hpp"
#include "render/card.hpp"
#include "render/json.hpp"
#include "render/lists.hpp"
#include "search/fuzzy.hpp"
#include "search/search.hpp"

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

// Every v1 command (PLAN §3), grouped; at most 40 lines. Commands of later
// milestones are listed and say "not available yet" until they land.
constexpr std::string_view help_text =
    "cxxman - C++ standard library reference cards\n"
    "\n"
    "usage: cxxman [options] <name>     a card: sort, vector::erase, std::optional\n"
    "       cxxman                      the interactive finder (in a terminal)\n"
    "\n"
    "find:\n"
    "  -s, --search <words...>   search by task: cxxman -s remove duplicates\n"
    "  -r, --recipe <words...>   a recipe: cxxman -r split string\n"
    "  -c, --compare <a> <b>     which one to use: cxxman -c map unordered_map\n"
    "  -H, --header <header>     what a header has: cxxman -H algorithm\n"
    "  --new <std>               names added in a standard: cxxman --new 17\n"
    "  --list [--kind K] [--header H] [--since S]\n"
    "  --coverage [--missing <header>]\n"
    "\n"
    "card:\n"
    "  -e, --examples            every example, code unwrapped\n"
    "  --sig                     signatures only\n"
    "  -b, --brief               one line: name, header, summary\n"
    "  --include                 the #include line\n"
    "  --support                 compiler versions and feature-test macro\n"
    "  --url, --web              print or open the cppreference page\n"
    "  --copy [N]                copy example N (default 1)\n"
    "  --json                    JSON for editors and scripts\n"
    "\n"
    "display:\n"
    "  --std 11..26 [--strict]   mark (or hide) what is newer than your standard\n"
    "  --color auto|always|never, --theme dark|light|mono\n"
    "  --width N                 60 to 100 columns\n"
    "  --ascii, --no-hyperlinks, --pager auto|always|never\n"
    "\n"
    "other:\n"
    "  update [--check]          update cxxman from GitHub Releases\n"
    "  --config [init|path]      show, create or locate the config file\n"
    "  --completions <shell>     bash, zsh, fish or powershell\n"
    "  --changelog, --version, --help\n"
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

// Layout options and the styles for stdout and for stderr (never colored).
struct View {
  render::Options o;
  render::Style style, plain;
};

// OSC 8 links (PLAN §4.7), when color is on: `always` and `never` decide;
// `auto` wants a terminal on stdout that is known to show them.
bool links_on(const Settings& s, const Context& ctx) {
  switch (s.hyperlinks) {
    case Mode::Always: return true;
    case Mode::Never: return false;
    case Mode::Auto: break;
  }
  return ctx.term.out_tty && render::detect_hyperlinks(ctx.env);
}

View view_for(const Settings& s, const Context& ctx) {
  View v;
  v.o = {width_for(s, ctx), s.ascii.value_or(!ctx.term.utf8), s.std, s.strict};
  v.style.theme = v.plain.theme = s.theme;
  v.style.ascii = v.plain.ascii = v.o.ascii;
  if (color_on(s, ctx)) {
    v.style.depth = render::detect_depth(ctx.env);
    v.style.hyperlinks = links_on(s, ctx);
  }
  return v;
}

// Asks main() to open `url` (--web).
int open_page(const std::string& url, Output& io) {
  if (!allowed_url(url)) {
    io.err += "cxxman: internal error: refusing to open " + url + "\n";
    return 3;
  }
  io.open_url = url;
  return 0;
}

// `cxxman <name>` with its modifier (PLAN §3.2) on a card.
int show_card(const Invocation& inv, const Catalog& cat, const Entry& e,
              std::span<const std::string_view> also, const View& v, Output& io) {
  auto none = [&](std::string_view what) {
    io.err += "cxxman: '" + std::string(e.id) + "' has no " + std::string(what) + "\n";
    return 1;
  };
  switch (inv.show) {
    case Show::Card: io.out += render::paint(render::card(cat, e, also, v.o), v.style); return 0;
    case Show::Examples:
      if (e.examples.empty()) return none("examples");
      io.out += render::paint(render::examples_only(e, v.o), v.style);
      return 0;
    case Show::Signatures:
      if (e.signatures.empty()) return none("signatures");
      io.out += render::paint(render::signatures_only(e, v.o), v.style);
      return 0;
    case Show::Brief: io.out += render::paint(render::brief(e, v.o), v.style); return 0;
    case Show::Include: io.out += "#include <" + std::string(e.header) + ">\n"; return 0;
    case Show::Url: io.out += render::reference_url(e.url) + "\n"; return 0;
    case Show::Web: return open_page(render::reference_url(e.url), io);
    case Show::Copy: {
      if (e.examples.empty()) return none("examples");
      const auto n = static_cast<std::size_t>(inv.copy);
      if (n > e.examples.size()) {
        io.err += "cxxman: '" + std::string(e.id) + "' has " + std::to_string(e.examples.size()) +
                  (e.examples.size() == 1 ? " example" : " examples") + "; --copy takes 1 to " +
                  std::to_string(e.examples.size()) + "\n";
        return 2;
      }
      const std::string_view code = e.examples[n - 1].code;
      const auto lines = static_cast<std::size_t>(std::ranges::count(code, '\n')) + 1;
      io.clipboard = std::string(code);
      io.copied = "example " + std::to_string(n) + " (" + std::to_string(lines) +
                  (lines == 1 ? " line)" : " lines)");
      return 0;
    }
  }
  return 3;
}

// A known name without a card: only its header and URL are known.
int show_index(const Invocation& inv, const Catalog& cat, const Resolution& r, const View& v,
               Output& io) {
  if (inv.show == Show::Card) {
    io.out += render::paint(render::index_only(cat, r.rows, r.also, v.o), v.style);
    return 0;
  }
  if (inv.show != Show::Include && inv.show != Show::Url && inv.show != Show::Web) {
    io.err += "cxxman: '" + inv.name + "' has no card yet; cxxman " + inv.name + " --url gives its page\n";
    return 1;
  }
  if (r.rows.size() > 1) {
    io.err += "cxxman: '" + inv.name + "' is in " + std::to_string(r.rows.size()) +
              " headers; cxxman " + inv.name + " lists them\n";
    return 2;
  }
  const IndexName& n = cat.index[static_cast<std::size_t>(r.rows.front())];
  if (inv.show == Show::Web) return open_page(render::reference_url(n.url), io);
  io.out += inv.show == Show::Include ? "#include <" + std::string(n.header) + ">\n"
                                      : render::reference_url(n.url) + "\n";
  return 0;
}

int show_name(const Invocation& inv, const Context& ctx, Output& io) {
  View v = view_for(inv.settings, ctx);
  v.o.support = inv.support;
  const Resolution r = resolve(ctx.catalog, inv.name);
  if (inv.json) {
    switch (r.what) {
      case Resolution::What::Card:
        io.out += render::json::card(ctx.catalog.entries[static_cast<std::size_t>(r.entry)], r.also);
        return 0;
      case Resolution::What::Index: io.out += render::json::index_only(ctx.catalog, r.rows, r.also); return 0;
      case Resolution::What::Ambiguous: io.out += render::json::ambiguous(inv.name, r.also); return 2;
      case Resolution::What::Missing: break;
    }
    io.out += render::json::missing(inv.name, r.suggestions);
    return 1;
  }
  switch (r.what) {
    case Resolution::What::Card:
      return show_card(inv, ctx.catalog, ctx.catalog.entries[static_cast<std::size_t>(r.entry)], r.also,
                       v, io);
    case Resolution::What::Index: return show_index(inv, ctx.catalog, r, v, io);
    case Resolution::What::Ambiguous:
      io.err += render::paint(render::ambiguous(inv.name, r.also, v.o), v.plain);
      return 2;
    case Resolution::What::Missing:
      break;
  }
  io.err += render::paint(render::not_found(inv.name, r.suggestions, v.o), v.plain);
  return 1;
}

int show_search(const Invocation& inv, const Context& ctx, Output& io) {
  const View v = view_for(inv.settings, ctx);
  const auto hits = search::search(ctx.catalog, inv.name);
  if (inv.json) {
    io.out += render::json::search(ctx.catalog, inv.name, hits);
    return hits.empty() ? 1 : 0;
  }
  if (hits.empty()) {
    io.err += "cxxman: no results for '" + inv.name + "'\n";
    return 1;
  }
  io.out += render::paint(render::search_results(ctx.catalog, hits, v.o), v.style);
  return 0;
}

std::vector<NameInfo> names_in(std::span<const NameInfo> all, std::string_view header) {
  std::vector<NameInfo> out;
  for (const auto& n : all)
    if (n.header == header) out.push_back(n);
  return out;
}

const Header* header_file(const Catalog& cat, std::string_view header) {
  auto it = std::ranges::lower_bound(cat.headers, header, {}, &Header::header);
  return it != cat.headers.end() && it->header == header ? &*it : nullptr;
}

// The known header closest to a mistyped one, or "".
std::string_view closest_header(std::string_view header, std::span<const NameInfo> all) {
  std::string_view best;
  int best_d = 3;
  for (const auto& n : all) {
    const int d = search::edit_distance(header, n.header, 2);
    if (d < best_d) {
      best_d = d;
      best = n.header;
    }
  }
  return best;
}

// "cxxman: no header <x>", with the closest known header as a hint.
int no_header(std::string_view header, std::span<const NameInfo> all, Output& io) {
  const std::string_view best = closest_header(header, all);
  io.err += "cxxman: no header <" + std::string(header) + ">";
  if (!best.empty()) io.err += " (did you mean <" + std::string(best) + ">?)";
  io.err += '\n';
  return 1;
}

int show_header(const Invocation& inv, const Context& ctx, Output& io) {
  const View v = view_for(inv.settings, ctx);
  const auto all = known_names(ctx.catalog);
  const std::string header = normalize_header(inv.header);
  const auto names = names_in(all, header);
  const Header* h = header_file(ctx.catalog, header);
  if (inv.json) {
    if (names.empty() && !h) {
      const std::string_view best = closest_header(header, all);
      io.out += render::json::missing(header, best.empty() ? std::span<const std::string_view>{}
                                                           : std::span<const std::string_view>(&best, 1));
      return 1;
    }
    io.out += render::json::header(header, h, names);
    return 0;
  }
  if (names.empty() && !h) return no_header(header, all, io);
  if (inv.show == Show::Web) return open_page(render::header_url(header), io);
  io.out += render::paint(render::header_index(ctx.catalog, header, h, names, v.o), v.style);
  return 0;
}

int show_list(const Invocation& inv, const Context& ctx, Output& io) {
  const View v = view_for(inv.settings, ctx);
  const std::string header = normalize_header(inv.header);
  std::vector<NameInfo> names;
  for (const auto& n : known_names(ctx.catalog)) {
    if (!header.empty() && n.header != header) continue;
    if (inv.kind && n.kind != *inv.kind) continue;
    if (inv.since != Std::Unknown && (n.since == Std::Unknown || n.since < inv.since)) continue;
    names.push_back(n);
  }
  if (names.empty()) {
    io.err += "cxxman: no names match\n";
    return 1;
  }
  io.out += render::paint(render::name_list(ctx.catalog, names, v.o), v.style);
  return 0;
}

int show_new(const Invocation& inv, const Context& ctx, Output& io) {
  const View v = view_for(inv.settings, ctx);
  std::vector<NameInfo> names;
  for (const auto& n : known_names(ctx.catalog))
    if (n.since == inv.new_std) names.push_back(n);
  if (names.empty()) {
    io.err += "cxxman: no names known for " + std::string(to_string(inv.new_std)) + " yet\n";
    return 1;
  }
  io.out += render::paint(render::whats_new(ctx.catalog, names, inv.new_std, v.o), v.style);
  return 0;
}

int show_coverage(const Invocation& inv, const Context& ctx, Output& io) {
  const View v = view_for(inv.settings, ctx);
  const auto all = known_names(ctx.catalog);
  if (inv.missing.empty()) {
    io.out += render::paint(render::coverage(all, v.o), v.style);
    return 0;
  }
  const std::string header = normalize_header(inv.missing);
  const auto names = names_in(all, header);
  if (names.empty()) return no_header(header, all, io);
  io.out += render::paint(render::missing(header, names, v.o), v.style);
  return 0;
}

}  // namespace

bool allowed_url(std::string_view url) {
  constexpr std::string_view prefix = "https://en.cppreference.com/";
  if (!url.starts_with(prefix) || url.size() == prefix.size()) return false;
  return std::ranges::all_of(url.substr(prefix.size()), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           std::string_view("/_.-%#~").find(c) != std::string_view::npos;
  });
}

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

  // Text for people may be paged; JSON is for programs, --copy may add OSC 52.
  if (ctx.term.out_tty && !inv.json && inv.show != Show::Copy && inv.action != Action::Complete &&
      inv.settings.pager != Mode::Never)
    io.page = inv.settings.pager == Mode::Always ? Paging::Always : Paging::Auto;

  switch (inv.action) {
    case Action::Version: io.out += "cxxman " CXXMAN_VERSION "\n"; return 0;
    case Action::Help: io.out += help_text; return 0;
    case Action::Changelog: {
      const std::string_view text = data::changelog();
      io.out += "cxxman " CXXMAN_VERSION "\n\n";
      io.out += text;
      if (!text.empty() && !text.ends_with('\n')) io.out += '\n';
      return 0;
    }
    case Action::ConfigPath:
      io.out += (ctx.config_file.empty() ? std::string("(no config directory)") : ctx.config_file.string()) + "\n";
      return 0;
    case Action::ConfigShow: io.out += show_config(inv.settings, ctx); return 0;
    case Action::ConfigInit: return init_config(ctx, io);
    case Action::Card: return show_name(inv, ctx, io);
    case Action::Search: return show_search(inv, ctx, io);
    case Action::Header: return show_header(inv, ctx, io);
    case Action::List: return show_list(inv, ctx, io);
    case Action::New: return show_new(inv, ctx, io);
    case Action::Coverage: return show_coverage(inv, ctx, io);
    case Action::Completions: io.out += completion_script(inv.shell); return 0;
    case Action::Complete: io.out += complete(ctx.catalog, inv.prefix); return 0;
    case Action::Usage:
      // `cxxman` without a name (display options allowed) opens the finder when
      // stdin and stdout are terminals, unless the config says interactive = false.
      if (ctx.term.out_tty && ctx.term.in_tty && inv.settings.interactive) {
        const View v = view_for(inv.settings, ctx);
        io.finder = FinderRequest{v.style, v.o.ascii, inv.settings.std, inv.settings.strict};
        io.page = Paging::Off;
        return 0;
      }
      break;
  }
  io.err += usage_text;
  return 2;
}

}  // namespace cxxman::app
