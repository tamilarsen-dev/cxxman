// cxxman-gen: the in-memory form of data/, its budgets (PLAN §5), and the
// parse / lint / emit entry points.
#pragma once

#include "model/model.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cxxman::gen {

// PLAN §5. Cut content; never raise these.
namespace budget {
inline constexpr std::size_t summary = 72;
inline constexpr std::size_t gotcha = 72;
inline constexpr std::size_t verdict = 72;
inline constexpr std::size_t title = 60;  // recipe and compare titles
inline constexpr std::size_t signature = 60;
inline constexpr std::size_t signature_note = 50;
inline constexpr std::size_t complexity = 60;
inline constexpr std::size_t example_comment = 24;
inline constexpr std::size_t variant_title = 40;
inline constexpr std::size_t code_width = 64;
inline constexpr std::size_t example_lines = 6;
inline constexpr std::size_t variant_lines = 8;
inline constexpr std::size_t row_label = 20;
inline constexpr std::size_t row_value = 48;
inline constexpr std::size_t badges = 3;
inline constexpr std::size_t tags = 12;
inline constexpr std::size_t see_also = 6;
inline constexpr std::size_t signatures = 5;
inline constexpr std::size_t member_groups = 6;
inline constexpr std::size_t examples = 4;
inline constexpr std::size_t gotchas = 4;
inline constexpr std::size_t variants = 3;
inline constexpr std::size_t subjects_min = 2, subjects_max = 4;
inline constexpr std::size_t rows_min = 3, rows_max = 8;
inline constexpr std::size_t verdicts = 3;
}  // namespace budget

// Snippets compile at least at this standard (PLAN §5.5).
inline constexpr Std snippet_floor = Std::Cpp17;

struct Loc {
  std::string file;  // display path, e.g. data/entries/algorithm/sort.toml
  int line = 0;
};

struct Diag {
  Loc loc;
  bool error = true;
  std::string message;
};

class Diags {
 public:
  void error(const Loc& at, std::string message);
  void warn(const Loc& at, std::string message);
  int errors() const noexcept { return errors_; }
  const std::vector<Diag>& all() const noexcept { return list_; }

 private:
  std::vector<Diag> list_;
  int errors_ = 0;
};

// A string from TOML with the line its content starts on.
struct Text {
  std::string value;
  Loc loc;
  bool block = false;  // a multi-line string: line i of value is on loc.line + i
};

struct Signature {
  std::string text, note;
  Std since = Std::Unknown;
  Loc loc;
};

struct Snippet {
  std::string title, comment;
  Text code;
  Std std = Std::Unknown;
  Loc loc;
};

struct Gotcha {
  Level level = Level::Warn;
  std::string text;
  Loc loc;
};

struct Group {
  std::string title;
  std::vector<std::string> names;
  Std since = Std::Unknown;
  Loc loc;
};

struct Support {
  std::string gcc, clang, msvc, apple_clang, ftm, ftm_value;
  Loc loc;
};

struct Skip {
  std::string compiler, reason;
  Loc loc;
};

struct Test {
  std::vector<std::string> includes;
  Text global, prelude;
  bool chain = false;
  std::vector<Skip> skip;
  Loc loc;
};

struct Entry {
  std::string id, name, header, summary, complexity, url, parent, verified;
  Kind kind = Kind::Unknown;
  Std since = Std::Unknown;
  Support support;
  std::vector<std::string> badges, aliases, tags, see_also;
  std::vector<Signature> signatures;
  std::vector<Group> members;
  std::vector<Snippet> examples;
  std::vector<Gotcha> gotchas;
  Test test;
  Loc loc;
};

struct Recipe {
  std::string id, title, summary;
  std::vector<std::string> tags, uses;
  std::vector<Snippet> variants;
  std::vector<Gotcha> gotchas;
  Test test;
  Loc loc;
};

struct Row {
  std::string label;
  std::vector<std::string> values;
  Loc loc;
};

struct Compare {
  std::string id, title;
  std::vector<std::string> subjects, tags;
  std::vector<Row> rows;
  std::vector<std::string> verdicts;
  Loc loc;
};

struct Header {
  std::string header, summary;
  std::vector<Group> groups;
  Loc loc;
};

struct IndexName {
  std::string name, header, url;
  Kind kind = Kind::Unknown;
  Std since = Std::Unknown;
  Loc loc;
};

struct Data {
  // Prefix that turns a Loc::file into a path the compiler can open; used for
  // #line in snippets. Empty when the data came from strings (tests).
  std::filesystem::path root;
  std::vector<Entry> entries;
  std::vector<Recipe> recipes;
  std::vector<Compare> compares;
  std::vector<Header> headers;
  std::vector<IndexName> index;
};

// parse.cpp: one function per file type. `file` is the display path.
void parse_entry(std::string_view toml, const std::string& file, Data& out, Diags& d);
void parse_recipe(std::string_view toml, const std::string& file, Data& out, Diags& d);
void parse_compare(std::string_view toml, const std::string& file, Data& out, Diags& d);
void parse_header(std::string_view toml, const std::string& file, Data& out, Diags& d);
void parse_index(std::string_view tsv, const std::string& file, Data& out, Diags& d);
// Reads every file below data_dir (PLAN §5) and sorts the tables by id.
Data load(const std::filesystem::path& data_dir, Diags& d);
void sort_tables(Data& data);

// lint.cpp. `today` is YYYY-MM-DD.
void lint(const Data& data, std::string_view today, Diags& d);

// emit.cpp
std::string normalize_key(std::string_view name);  // trim, drop a leading std::
std::string emit_data_cpp(const Data& data, std::string_view changelog);

struct SnippetFile {
  std::string name;  // file stem, unique
  Std std = Std::Cpp17;
  std::vector<std::string> skip;  // compilers: gcc, clang, msvc
  std::string source;
};
std::vector<SnippetFile> make_snippets(const Data& data);
std::string emit_manifest(const std::vector<SnippetFile>& snippets);

// The body of the "## <version>" section of CHANGELOG.md, or nullopt.
std::optional<std::string> changelog_section(std::string_view markdown,
                                             std::string_view version);

std::size_t display_length(std::string_view utf8);  // code points

}  // namespace cxxman::gen
