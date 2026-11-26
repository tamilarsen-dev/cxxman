// cxxman-gen: parsing, linting and snippet generation on in-memory data.
#include "schema.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>

using namespace cxxman;
using namespace cxxman::gen;

namespace {

constexpr std::string_view index_tsv =
    "name\theader\tkind\tsince\turl\n"
    "frob\tutility\tfunction\t\tutility/frob\n"
    "twiddle\tutility\tfunction\tC++17\tutility/twiddle\n";

// Lines:            1
const std::string card = R"toml(id      = "frob"
name    = "std::frob"
kind    = "function"
header  = "utility"
since   = "C++11"
summary = "Frob a value."
url     = "utility/frob"
tags    = ["frob"]

[[signature]]
text = "frob(x)"

[[example]]
code = "frob(1);"

[[gotcha]]
level = "warn"
text  = "frob is a test fixture"

[test]
global = "void frob(int) {}"
)toml";

struct Result {
  Data data;
  Diags diags;

  bool has(std::string_view text, int line = 0, bool error = true) const {
    return std::ranges::any_of(diags.all(), [&](const Diag& d) {
      return d.error == error && d.message.find(text) != std::string::npos &&
             (line == 0 || d.loc.line == line);
    });
  }
};

Result run(const std::vector<std::string>& cards, std::string_view today = "2026-11-26") {
  Result r;
  parse_index(index_tsv, "data/index/names.tsv", r.data, r.diags);
  for (std::size_t i = 0; i < cards.size(); ++i)
    parse_entry(cards[i], "card" + std::to_string(i) + ".toml", r.data, r.diags);
  sort_tables(r.data);
  if (r.diags.errors() == 0) lint(r.data, today, r.diags);
  return r;
}

std::string replace(std::string s, std::string_view from, std::string_view to) {
  auto pos = s.find(from);
  REQUIRE(pos != std::string::npos);
  return s.replace(pos, from.size(), to);
}

}  // namespace

TEST_CASE("a valid card lints without errors or warnings except 'not verified'") {
  auto r = run({card});
  CHECK(r.diags.errors() == 0);
  REQUIRE(r.data.entries.size() == 1);
  CHECK(r.data.entries[0].since == Std::Cpp11);
  CHECK(r.has("not verified", 0, false));
  CHECK(r.diags.all().size() == 1);
}

TEST_CASE("an unknown field is an error at its line") {
  auto r = run({"colour = \"red\"\n" + card});
  CHECK(r.has("unknown field 'colour' in card", 1));
}

TEST_CASE("an unknown field in a nested table is an error") {
  auto r = run({replace(card, "text = \"frob(x)\"", "text = \"frob(x)\"\nnotes = \"x\"")});
  CHECK(r.has("unknown field 'notes' in [[signature]]", 12));
}

TEST_CASE("missing and mistyped fields") {
  CHECK(run({replace(card, "url     = \"utility/frob\"\n", "")}).has("missing required field 'url'"));
  CHECK(run({replace(card, "[test]\n", "[nottest]\n")}).has("missing required field 'test'"));
  CHECK(run({replace(card, "tags    = [\"frob\"]", "tags = \"frob\"")}).has("must be an array of strings"));
  CHECK(run({replace(card, "\"C++11\"", "\"C++12\"")}).has("unknown standard 'C++12'"));
  CHECK(run({replace(card, "\"C++11\"", "\"C++29\"")}).has("C++29 is reserved"));
  CHECK(run({replace(card, "\"function\"", "\"widget\"")}).has("unknown kind 'widget'"));
}

TEST_CASE("budgets") {
  const std::string long_summary(72, 'x');
  CHECK(run({replace(card, "Frob a value.", long_summary + ".")}).has("summary is 73 characters; the budget is 72"));
  CHECK(run({replace(card, "Frob a value.", "Frob a value")}).has("summary must end with '.'"));
  CHECK(run({replace(card, "code = \"frob(1);\"", "code = \"" + std::string(65, 'x') + "\"")})
            .has("example code line is 65 characters; the budget is 64", 14));
  CHECK(run({replace(card, "code = \"frob(1);\"", "code = \"1\\n2\\n3\\n4\\n5\\n6\\n7\"")})
            .has("example code has 7 lines; the budget is 6"));
  // Display width counts code points, not bytes.
  CHECK_FALSE(run({replace(card, "Frob a value.", std::string(35, 'x') + "\xE2\x89\xA4" + std::string(35, 'x') + ".")})
                  .has("summary is"));
}

TEST_CASE("code with a tab or trailing space") {
  auto r = run({replace(card, "code = \"frob(1);\"", "code = \"\"\"\nfrob(1);\n\tfrob(2); \"\"\"")});
  CHECK(r.has("contains a tab", 16));
  CHECK(r.has("trailing whitespace", 16));
}

TEST_CASE("cross references") {
  CHECK(run({card + "\n"}).diags.errors() == 0);
  auto see = replace(card, "tags    = [\"frob\"]", "tags = [\"frob\"]\nsee_also = [\"twiddle\", \"std::frob\", \"nope\"]");
  auto r = run({see});
  CHECK(r.has("see_also names 'nope'"));
  CHECK_FALSE(r.has("'twiddle'"));
  CHECK_FALSE(r.has("'std::frob'"));
  CHECK(run({replace(card, "header  = \"utility\"", "header  = \"nosuch\"")}).has("header <nosuch> is not in index"));
}

TEST_CASE("ids and aliases are unique across cards") {
  auto other = replace(replace(card, "id      = \"frob\"", "id = \"twiddle\"\naliases = [\"std::frob\"]"),
                       "std::frob\"\nkind", "std::twiddle\"\nkind");
  CHECK(run({card, other}).has("'std::frob' is already the id or an alias of card 'frob'"));
}

TEST_CASE("member cards need a class parent") {
  auto member = replace(replace(card, "\"function\"", "\"member\""), "id      = \"frob\"", "id = \"frob::size\"");
  CHECK(run({member}).has("a member card needs 'parent'"));
  CHECK(run({replace(member, "url ", "parent = \"frob\"\nurl ")}).has("parent 'frob' is not a class card"));
  CHECK(run({replace(card, "url ", "parent = \"x\"\nurl ")}).has("'parent' is only for kind = \"member\""));
}

TEST_CASE("verified dates, support data and skips") {
  auto dated = replace(card, "tags ", "verified = 2026-11-27\ntags ");
  CHECK(run({dated}, "2026-11-26").has("is in the future"));
  CHECK(run({dated}, "2026-11-27").diags.all().empty());
  CHECK(run({replace(card, "tags ", "verified = \"2026-11-01\"\ntags ")}).has("must be a date"));

  auto support = card + "\n[support]\ngcc = \"13\"\nmsvc = \"19.x\"\nftm = \"cpp_lib_frob\"\nftm_value = \"2019L\"\n";
  // [support] after [test] is a sibling table, which is what we want here.
  auto r = run({support});
  CHECK_FALSE(r.has("gcc"));
  CHECK(r.has("msvc '19.x' is not a version"));
  CHECK(r.has("is not a feature-test macro"));
  CHECK(r.has("must look like 201907L"));

  auto skip = card + "skip = [{ compiler = \"icc\" }]\n";
  auto s = run({skip});
  CHECK(s.has("skip compiler must be gcc, clang or msvc"));
  CHECK(s.has("skip needs a reason"));
}

TEST_CASE("snippets: #line points at the TOML, standard is the highest that applies") {
  auto multi = replace(card, "code = \"frob(1);\"", "code = \"\"\"\nfrob(1);\nfrob(2);\"\"\"\nstd = \"C++20\"");
  multi += "prelude = \"\"\"\nint a = 0;\nint b = 1;\"\"\"\n";
  auto r = run({multi});
  REQUIRE(r.diags.errors() == 0);
  auto snippets = make_snippets(r.data);
  REQUIRE(snippets.size() == 1);
  const auto& s = snippets[0];
  CHECK(s.name == "entry-frob-0");
  CHECK(s.std == Std::Cpp20);
  CHECK(s.source.find("#line 24 \"card0.toml\"\nvoid frob(int) {}\n") != std::string::npos);
  CHECK(s.source.find("#line 26 \"card0.toml\"\nint a = 0;\nint b = 1;\n") != std::string::npos);
  CHECK(s.source.find("#line 15 \"card0.toml\"\nfrob(1);\nfrob(2);\n") != std::string::npos);
  CHECK(emit_manifest(snippets) == "# Generated by cxxman-gen. Do not edit.\ncxxman_snippet(entry-frob-0 20 \"\")\n");
}

TEST_CASE("snippets: one-line strings with \\n map every line to the same TOML line") {
  auto r = run({replace(card, "code = \"frob(1);\"", "code = \"frob(1);\\nfrob(2);\"")});
  auto s = make_snippets(r.data).at(0).source;
  CHECK(s.find("#line 14 \"card0.toml\"\nfrob(1);\n#line 14 \"card0.toml\"\nfrob(2);\n") != std::string::npos);
}

TEST_CASE("snippets: chain includes earlier examples and their standard") {
  auto two = replace(card, "[[gotcha]]",
                     "std = \"C++23\"\n\n[[example]]\ncode = \"frob(2);\"\n\n[[gotcha]]");
  two = replace(two, "[test]\n", "[test]\nchain = true\n");
  auto r = run({two});
  REQUIRE(r.diags.errors() == 0);
  auto s = make_snippets(r.data);
  REQUIRE(s.size() == 2);
  CHECK(s[1].std == Std::Cpp23);
  CHECK(s[1].source.find("frob(1);") != std::string::npos);
  CHECK(s[1].source.find("frob(2);") != std::string::npos);
  CHECK(s[0].source.find("frob(2);") == std::string::npos);
}

TEST_CASE("index rows") {
  Data data;
  Diags d;
  parse_index("name\theader\tkind\tsince\turl\nx\ty\tgadget\t\tz\nonly\tthree\tcols\n", "n.tsv", data, d);
  CHECK(d.errors() == 2);
  Diags d2;
  parse_index("wrong header\n", "n.tsv", data, d2);
  CHECK(d2.errors() == 1);
}

TEST_CASE("normalize_key and changelog_section") {
  CHECK(normalize_key("  std::ranges::sort ") == "ranges::sort");
  CHECK(normalize_key("vector::erase") == "vector::erase");

  constexpr std::string_view md =
      "# Changelog\n\n## 1.1.0\n\n### App\n\n- new\n\n## 1.0.0 (2026-12-01)\n\n- old\n";
  CHECK(changelog_section(md, "1.1.0") == "### App\n\n- new");
  CHECK(changelog_section(md, "1.0.0") == "- old");
  CHECK_FALSE(changelog_section(md, "1.0").has_value());
  CHECK_FALSE(changelog_section(md, "2.0.0").has_value());
}

TEST_CASE("data.cpp marks index names covered by a card") {
  auto r = run({replace(card, "tags ", "aliases = [\"twiddle\"]\ntags ")});
  REQUIRE(r.diags.errors() == 0);
  auto cpp = emit_data_cpp(r.data, "notes");
  CHECK(cpp.find("{\"frob\"sv, \"utility\"sv, \"utility/frob\"sv, Kind::Function, Std::Unknown, 0}") != std::string::npos);
  CHECK(cpp.find("{\"twiddle\"sv, \"utility\"sv, \"utility/twiddle\"sv, Kind::Function, Std::Cpp17, 0}") != std::string::npos);
  CHECK(cpp.find("changelog_text = \"notes\"sv") != std::string::npos);
}
