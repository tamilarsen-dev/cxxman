// Options, CXXMAN_OPTS / CXXMAN_STD, the config file, and how they combine
// (PLAN §3.3, §7.5).
#include "app/app.hpp"
#include "app/config.hpp"
#include "app/options.hpp"
#include "render/layout.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using namespace cxxman;
using namespace cxxman::app;

namespace {

struct Env {
  std::map<std::string, std::string> vars;
  render::Getenv get() const {
    return [this](const char* n) -> const char* {
      auto it = vars.find(n);
      return it == vars.end() ? nullptr : it->second.c_str();
    };
  }
};

Invocation parse_with(std::vector<std::string> args, const Env& env = {},
                      std::string_view config_text = {}) {
  ConfigFile cfg = parse_config(config_text, "config.toml");
  return parse(args, env.get(), config_text.empty() ? nullptr : &cfg);
}

}  // namespace

TEST_CASE("config parser") {
  auto c = parse_config(
      "# comment\n"
      "theme = \"light\"  # trailing comment\n"
      "width=90\n"
      "ascii = true\n"
      "color = always\n"
      "bad line\n"
      "Theme = x\n"
      "pager = \"never\n",
      "c.toml");
  REQUIRE(c.entries.size() == 4);
  CHECK(c.entries[0].key == "theme");
  CHECK(c.entries[0].value == "light");
  CHECK(c.entries[1].value == "90");
  CHECK(c.entries[3].value == "always");
  REQUIRE(c.problems.size() == 3);
  CHECK(c.problems[0] == "c.toml:6: expected key = value");
  CHECK(c.problems[1] == "c.toml:7: bad key 'Theme'");
  CHECK(c.problems[2] == "c.toml:8: bad string value for 'pager'");
  CHECK(parse_config(default_config(), "d").problems.empty());
  CHECK(parse_config(default_config(), "d").entries.empty());
}

TEST_CASE("precedence: flag > CXXMAN_OPTS > config > default") {
  const std::string cfg = "theme = \"light\"\nwidth = 70\nstd = 14\nascii = true\n";
  Env env{{{"CXXMAN_OPTS", "--width 80 --theme mono"}, {"CXXMAN_STD", "17"}}};
  auto inv = parse_with({"sort", "--width=90"}, env, cfg);
  REQUIRE(inv.error.empty());
  CHECK(inv.action == Action::Card);
  CHECK(inv.name == "sort");
  CHECK(inv.settings.width == 90);
  CHECK(inv.settings.source_of("width") == Source::Flag);
  CHECK(inv.settings.theme == render::Theme::Mono);
  CHECK(inv.settings.source_of("theme") == Source::Env);
  CHECK(inv.settings.std == Std::Cpp17);
  CHECK(inv.settings.source_of("std") == Source::Env);
  CHECK(inv.settings.ascii == true);
  CHECK(inv.settings.source_of("ascii") == Source::Config);
  CHECK(inv.settings.source_of("color") == Source::Default);

  // CXXMAN_OPTS beats CXXMAN_STD.
  env.vars["CXXMAN_OPTS"] = "--std C++20";
  CHECK(parse_with({"x"}, env).settings.std == Std::Cpp20);
}

TEST_CASE("bad config values warn and keep the default") {
  auto inv = parse_with({"sort"}, {}, "theme = \"neon\"\nfrobs = 1\n");
  CHECK(inv.error.empty());
  CHECK(inv.settings.theme == render::Theme::Dark);
  REQUIRE(inv.warnings.size() == 2);
  CHECK(inv.warnings[0] == "config.toml:1: theme must be dark, light or mono, not 'neon'; using the default");
  CHECK(inv.warnings[1] == "config.toml:2: unknown key 'frobs'");
}

TEST_CASE("usage errors") {
  CHECK(parse_with({"--sgi"}).error == "unknown option '--sgi' (did you mean --sig?)");
  CHECK(parse_with({"--colour", "never"}).error == "unknown option '--colour' (did you mean --color?)");
  CHECK(parse_with({"--json"}).error == "'--json' is not available yet");
  CHECK(parse_with({"--width"}).error == "--width needs a value");
  CHECK(parse_with({"--width", "wide"}).error == "--width: width must be a number of columns, not 'wide'");
  CHECK(parse_with({"--ascii=yes"}).error == "--ascii takes no value");
  CHECK(parse_with({"update"}).error == "'update' is not available yet");
  CHECK(parse_with({"x"}, Env{{{"CXXMAN_OPTS", "sort"}}}).error == "CXXMAN_OPTS: 'sort' is not an option");
  CHECK(parse_with({"x"}, Env{{{"CXXMAN_STD", "18"}}}).error ==
        "CXXMAN_STD: std must be 11, 14, 17, 20, 23 or 26, not '18'");
}

TEST_CASE("actions and names") {
  CHECK(parse_with({}).action == Action::Usage);
  CHECK(parse_with({"--version", "sort"}).action == Action::Version);
  CHECK(parse_with({"--config"}).action == Action::ConfigShow);
  CHECK(parse_with({"--config", "init"}).action == Action::ConfigInit);
  CHECK(parse_with({"--config", "path"}).action == Action::ConfigPath);
  CHECK(parse_with({"operator", "new"}).name == "operator new");
  CHECK(parse_with({"--", "--weird"}).name == "--weird");
}

TEST_CASE("run: color, width and the config file") {
  auto run_with = [](std::vector<std::string> args, Env env, Terminal term,
                     std::filesystem::path config = {}) {
    Context ctx{env.get(), term, Catalog::builtin(), config};
    Output io;
    const int code = run(args, ctx, io);
    return std::tuple(code, io.out, io.err);
  };
  const Terminal tty{true, 120, true, true};
  const Terminal pipe{false, 0, true, true};

  auto [c1, out1, e1] = run_with({"sort"}, {}, pipe);
  CHECK(c1 == 0);
  CHECK(out1.find('\x1b') == std::string::npos);  // piped: no escapes

  auto [c2, out2, e2] = run_with({"sort"}, {}, tty);
  CHECK(out2.find('\x1b') != std::string::npos);
  auto [c2b, out2b, e2b] = run_with({"sort", "--color", "never"}, {}, tty);
  // A 120-column terminal gets the 100-column maximum.
  CHECK(render::display_width(out2b.substr(0, out2b.find('\n'))) == 100);

  auto [c3, out3, e3] = run_with({"sort"}, Env{{{"NO_COLOR", "1"}}}, tty);
  CHECK(out3.find('\x1b') == std::string::npos);
  auto [c4, out4, e4] = run_with({"sort"}, Env{{{"CLICOLOR_FORCE", "1"}}}, pipe);
  CHECK(out4.find('\x1b') != std::string::npos);
  auto [c5, out5, e5] = run_with({"sort", "--color", "always"}, Env{{{"NO_COLOR", "1"}}}, pipe);
  CHECK(out5.find('\x1b') != std::string::npos);

  // ASCII when the console cannot show UTF-8.
  auto [c6, out6, e6] = run_with({"sort", "--color", "never"}, {}, Terminal{true, 80, true, false});
  CHECK(out6.find("C++98 | constexpr") != std::string::npos);

  // --config init writes the file once.
  const auto dir = std::filesystem::temp_directory_path() / "cxxman-test-config";
  std::filesystem::remove_all(dir);
  const auto file = dir / "config.toml";
  auto [c7, out7, e7] = run_with({"--config", "init"}, {}, pipe, file);
  CHECK(c7 == 0);
  CHECK(std::filesystem::exists(file));
  auto [c8, out8, e8] = run_with({"--config", "init"}, {}, pipe, file);
  CHECK(c8 == 2);
  CHECK(e8.find("already exists") != std::string::npos);
  {
    std::ofstream(file, std::ios::trunc) << "width = 60\ntheme = neon\n";
  }
  auto [c9, out9, e9] = run_with({"--config"}, {}, pipe, file);
  CHECK(c9 == 0);
  const auto at = out9.find("  width ");
  REQUIRE(at != std::string::npos);
  const auto line = out9.substr(at, out9.find('\n', at) - at);
  CHECK(line.find(" 60 ") != std::string::npos);
  CHECK(line.ends_with("config"));
  CHECK(e9.find("theme must be dark, light or mono") != std::string::npos);
  std::filesystem::remove_all(dir);
}
