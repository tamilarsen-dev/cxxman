// Golden output of `cxxman <args>` (PLAN §10). Each case runs app::run with a
// piped terminal and compares stdout, stderr and the exit code with
// tests/golden/<case>.txt. CXXMAN_UPDATE_GOLDEN=1 rewrites the files; read
// every diff before committing it.
#include "app/app.hpp"
#include "fixture_catalog.hpp"
#include "sys/paths.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

using namespace cxxman;

namespace {

struct Case {
  std::string file;
  std::vector<std::string> args{};
  std::vector<int> widths{};  // one golden per width; none: args carry it
  std::map<std::string, std::string> env{};
  bool fixture = false;
};

std::string read(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::string s{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  std::erase(s, '\r');
  return s;
}

// Escape codes as "\e" so the files stay readable in a diff.
std::string visible(std::string s) {
  std::string out;
  for (char c : s) {
    if (c == '\x1b')
      out += "\\e";
    else
      out += c;
  }
  return out;
}

std::string run_case(const Case& c, const std::vector<std::string>& args) {
  app::Context ctx;
  ctx.env = [&c](const char* n) -> const char* {
    auto it = c.env.find(n);
    return it == c.env.end() ? nullptr : it->second.c_str();
  };
  ctx.term = {false, 0, true, true};
  ctx.catalog = c.fixture ? fixture::catalog() : Catalog::builtin();
  app::Output io;
  const int code = app::run(args, ctx, io);
  std::string text = "$ cxxman";
  for (const auto& a : args) text += " " + a;
  text += "\nexit " + std::to_string(code) + "\n" + visible(io.out);
  if (!io.err.empty()) text += "--- stderr\n" + visible(io.err);
  return text;
}

void check(const std::string& name, const std::string& actual) {
  const auto path = std::filesystem::path(CXXMAN_GOLDEN_DIR) / (name + ".txt");
  const char* update = sys::getenv("CXXMAN_UPDATE_GOLDEN");
  if (update && std::string_view(update) == "1") {
    std::ofstream(path, std::ios::binary | std::ios::trunc) << actual;
    return;
  }
  const std::string expected = read(path);
  INFO("golden: ", path.string(), " (CXXMAN_UPDATE_GOLDEN=1 rewrites it)");
  if (expected.empty()) FAIL("missing golden file");
  if (expected == actual) return;
  // Show the first line that differs.
  std::size_t line = 1, i = 0;
  while (i < expected.size() && i < actual.size() && expected[i] == actual[i])
    if (expected[i++] == '\n') ++line;
  const auto line_of = [&](const std::string& s) {
    auto start = s.rfind('\n', i == 0 ? 0 : i - 1);
    start = start == std::string::npos ? 0 : start + 1;
    return s.substr(start, s.find('\n', start) - start);
  };
  FAIL_CHECK("line " << line << "\n  expected: " << line_of(expected) << "\n  actual:   "
                     << line_of(actual));
}

const std::vector<Case> cases = {
    // Every seed card at the three widths (PLAN §4.10).
    {"card-sort", {"sort"}, {60, 80, 100}},
    {"card-unique", {"unique"}, {60, 80, 100}},
    {"card-vector", {"vector"}, {60, 80, 100}},
    {"card-vector-erase", {"vector::erase"}, {60, 80, 100}},
    {"card-optional", {"optional"}, {60, 80, 100}},
    // One card with the escape codes locked down, and one in ASCII.
    {"card-sort-dark", {"sort", "--color", "always", "--theme", "dark", "--width", "80"}, {},
     {{"COLORTERM", "truecolor"}}},
    {"card-sort-ascii", {"sort", "--ascii", "--width", "80"}, {}},
    // Known names without a card.
    {"index-istringstream", {"istringstream"}, {80}},
    {"index-erase", {"erase"}, {60, 80}},
    {"index-swap", {"swap"}, {100}},
    // A bare member of one class; a miss; an ambiguity (fixture).
    {"member-push_back", {"push_back"}, {80}},
    {"miss-srot", {"srot"}, {80}},
    {"miss-nothing", {"zzzz"}, {80}},
    {"ambiguous-size", {"size"}, {80}, {}, true},
};

}  // namespace

TEST_CASE("golden output") {
  for (const auto& c : cases) {
    if (c.widths.empty()) {
      check(c.file, run_case(c, c.args));
      continue;
    }
    for (int w : c.widths) {
      auto args = c.args;
      args.insert(args.end(), {"--width", std::to_string(w), "--color", "never"});
      check(c.file + "." + std::to_string(w), run_case(c, args));
    }
  }
}
