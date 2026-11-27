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
  bool tty = false;  // stdout is a terminal (120 columns)
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
  ctx.term = {c.tty, c.tty ? 120u : 0u, true, true};
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
    // Task search: the acceptance queries (PLAN §8.3), ASCII markers, a miss.
    {"search-remove-duplicates", {"-s", "remove", "duplicates"}, {60, 80, 100}},
    {"search-dynamic-array", {"-s", "dynamic", "array"}, {60, 80, 100}},
    {"search-erase-element", {"-s", "erase", "element"}, {60, 80, 100}},
    {"search-maybe-value", {"-s", "maybe", "value"}, {60, 80, 100}},
    {"search-split-string", {"-s", "split", "string"}, {60, 80, 100}},
    {"search-hash-map", {"-s", "hash", "map"}, {60, 80, 100}},
    {"search-hash-map-ascii", {"-s", "hash", "map", "--ascii", "--width", "80"}, {}},
    {"search-nothing", {"-s", "zzzz"}, {80}},
    // Header index, with and without a header file; a typo.
    {"header-algorithm", {"-H", "algorithm"}, {60, 80, 100}},
    {"header-vector", {"-H", "<vector>"}, {80}},
    {"header-typo", {"-H", "algoritm"}, {80}},
    // --std marking and --strict hiding (PLAN §4.3); --std 17 on sort is required (PLAN §10).
    {"card-sort-std17", {"sort", "--std", "17"}, {60, 80, 100}},
    {"card-sort-std17-strict", {"sort", "--std", "17", "--strict"}, {60, 80}},
    {"card-sort-std17-ascii", {"sort", "--std", "17", "--ascii", "--width", "80"}, {}},
    {"card-optional-std14", {"optional", "--std", "14"}, {80}},
    // Card modifiers (PLAN §3.2).
    {"examples-unique", {"unique", "-e"}, {60, 80}},
    {"examples-optional-strict", {"optional", "-e", "--std", "17", "--strict"}, {80}},
    {"sig-sort", {"sort", "--sig"}, {60, 80}},
    {"brief-sort", {"sort", "-b"}, {60, 80}},
    {"brief-vector-ascii", {"vector", "--brief", "--ascii", "--width", "60"}, {}},
    {"include-sort", {"sort", "--include"}, {80}},
    {"url-vector-erase", {"vector::erase", "--url"}, {80}},
    {"include-istringstream", {"istringstream", "--include"}, {80}},
    {"url-istringstream", {"istringstream", "--url"}, {80}},
    {"url-erase", {"erase", "--url"}, {80}},
    {"examples-istringstream", {"istringstream", "-e"}, {80}},
    {"support-sort", {"sort", "--support"}, {80}},
    {"support-fixture", {"vector", "--support"}, {60, 80}, {}, true},
    {"support-fixture-std20", {"vector", "--std", "20"}, {80}, {}, true},
    {"support-fixture-std17", {"vector", "--std", "17"}, {80}, {}, true},
    {"help", {"--help"}, {}},
    // OSC 8 links in a terminal known to show them (PLAN §4.7).
    {"card-sort-links", {"sort", "--color", "always", "--width", "80"}, {},
     {{"COLORTERM", "truecolor"}, {"WT_SESSION", "1"}}, false, true},
    {"header-vector-links", {"-H", "vector", "--color", "always", "--width", "80"}, {},
     {{"WT_SESSION", "1"}}, false, true},
    // --json (docs/JSON.md): every name result, search, -H.
    {"json-sort", {"sort", "--json"}, {}},
    {"json-vector", {"vector", "--json"}, {}},
    {"json-istringstream", {"istringstream", "--json"}, {}},
    {"json-erase", {"erase", "--json"}, {}},
    {"json-ambiguous-size", {"size", "--json"}, {}, {}, true},
    {"json-support-fixture", {"vector", "--json"}, {}, {}, true},
    {"json-miss-srot", {"srot", "--json"}, {}},
    {"json-search-remove-duplicates", {"-s", "remove", "duplicates", "--json"}, {}},
    {"json-search-split-string", {"--json", "-s", "split", "string"}, {}},
    {"json-search-nothing", {"-s", "zzzz", "--json"}, {}},
    {"json-header-vector", {"-H", "vector", "--json"}, {}},
    {"json-header-typo", {"-H", "algoritm", "--json"}, {}},
    // --new, --list, --coverage.
    {"new-17", {"--new", "17"}, {60, 80, 100}},
    {"list-vector", {"--list", "--header", "vector"}, {80}},
    {"coverage", {"--coverage"}, {80}},
    {"coverage-missing-vector", {"--coverage", "--missing", "vector"}, {80}},
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
