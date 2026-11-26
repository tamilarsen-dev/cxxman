// Reads data/ (PLAN §5) into gen::Data. Structure and types are checked here;
// budgets and cross-references are lint.cpp's job.
#include "schema.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <sstream>

namespace cxxman::gen {

void Diags::error(const Loc& at, std::string message) {
  list_.push_back({at, true, std::move(message)});
  ++errors_;
}

void Diags::warn(const Loc& at, std::string message) {
  list_.push_back({at, false, std::move(message)});
}

std::size_t display_length(std::string_view utf8) {
  return static_cast<std::size_t>(std::count_if(utf8.begin(), utf8.end(), [](char c) {
    return (static_cast<unsigned char>(c) & 0xC0) != 0x80;
  }));
}

namespace {

std::string_view line_of(std::string_view text, int line) {
  std::size_t pos = 0;
  for (int i = 1; i < line; ++i) {
    pos = text.find('\n', pos);
    if (pos == std::string_view::npos) return {};
    ++pos;
  }
  auto end = text.find('\n', pos);
  return text.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
}

// toml++ columns count code points; turn one into a byte offset.
std::size_t byte_at_column(std::string_view line, int column) {
  std::size_t i = 0;
  for (int col = 1; i < line.size() && col < column; ++col) {
    ++i;
    while (i < line.size() && (static_cast<unsigned char>(line[i]) & 0xC0) == 0x80) ++i;
  }
  return i;
}

std::string squote(std::string_view s) { return "'" + std::string(s) + "'"; }

Std parse_std(std::string_view s, const Loc& at, std::string_view key, Diags& d) {
  auto v = enum_from<Std>(std_names, s);
  if (!v) {
    d.error(at, "field '" + std::string(key) + "' has unknown standard " + squote(s) +
                    " (use C++98, C++11, ... " + std::string(to_string(latest_std)) + ")");
    return Std::Unknown;
  }
  if (*v > latest_std) {
    d.error(at, std::string(to_string(*v)) + " is reserved; the newest allowed is " +
                    std::string(to_string(latest_std)));
    return Std::Unknown;
  }
  return *v;
}

// Field access for one TOML table. Reports unknown keys on construction, and
// missing or mistyped fields as they are read.
class Reader {
 public:
  Reader(const toml::table& t, std::string_view src, const std::string& file, Diags& d,
         std::string_view what, std::initializer_list<std::string_view> allowed)
      : t_(t), src_(src), file_(file), d_(d), what_(what) {
    for (auto&& [k, v] : t) {
      if (std::find(allowed.begin(), allowed.end(), k.str()) == allowed.end())
        d_.error(at(k.source()), "unknown field " + squote(k.str()) + " in " + what_);
    }
  }

  Loc loc() const { return at(t_.source()); }

  Loc at(const toml::source_region& r) const {
    return {file_, r.begin.line ? static_cast<int>(r.begin.line) : 1};
  }

  Loc key_loc(std::string_view key) const {
    auto* n = t_.get(key);
    return n ? at(n->source()) : loc();
  }

  std::string str(std::string_view key, bool required) {
    auto* n = t_.get(key);
    if (!n) {
      if (required) missing(key);
      return {};
    }
    if (auto* s = n->as_string()) return s->get();
    mistyped(*n, key, "a string");
    return {};
  }

  Text text(std::string_view key, bool required) {
    Text out;
    out.loc = loc();
    auto* n = t_.get(key);
    if (!n) {
      if (required) missing(key);
      return out;
    }
    auto* s = n->as_string();
    if (!s) {
      mistyped(*n, key, "a string");
      return out;
    }
    out.value = s->get();
    const int line = static_cast<int>(n->source().begin.line);
    out.loc = {file_, line};
    auto rest = line_of(src_, line).substr(
        byte_at_column(line_of(src_, line), static_cast<int>(n->source().begin.column)));
    if (rest.starts_with("\"\"\"") || rest.starts_with("'''")) {
      out.block = true;
      // TOML drops a newline right after the opening delimiter.
      if (rest.size() == 3) out.loc.line = line + 1;
    }
    return out;
  }

  std::vector<std::string> strs(std::string_view key, bool required) {
    std::vector<std::string> out;
    auto* n = t_.get(key);
    if (!n) {
      if (required) missing(key);
      return out;
    }
    auto* a = n->as_array();
    if (!a) {
      mistyped(*n, key, "an array of strings");
      return out;
    }
    for (auto&& el : *a) {
      if (auto* s = el.as_string())
        out.push_back(s->get());
      else
        mistyped(el, key, "an array of strings");
    }
    return out;
  }

  bool boolean(std::string_view key) {
    auto* n = t_.get(key);
    if (!n) return false;
    if (auto* b = n->as_boolean()) return b->get();
    mistyped(*n, key, "true or false");
    return false;
  }

  Std std_value(std::string_view key, bool required) {
    auto s = str(key, required);
    return s.empty() ? Std::Unknown : parse_std(s, key_loc(key), key, d_);
  }

  std::string date(std::string_view key) {
    auto* n = t_.get(key);
    if (!n) return {};
    auto* v = n->as_date();
    if (!v) {
      mistyped(*n, key, "a date like 2026-11-26 (no quotes)");
      return {};
    }
    const toml::date& dt = v->get();
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02u-%02u", static_cast<int>(dt.year),
                  static_cast<unsigned>(dt.month), static_cast<unsigned>(dt.day));
    return buf;
  }

  const toml::table* table(std::string_view key, bool required) {
    auto* n = t_.get(key);
    if (!n) {
      if (required) missing(key);
      return nullptr;
    }
    if (auto* t = n->as_table()) return t;
    mistyped(*n, key, "a table");
    return nullptr;
  }

  // [[key]] arrays of tables.
  template <class F>
  void tables(std::string_view key, F&& each) {
    auto* n = t_.get(key);
    if (!n) return;
    auto* a = n->as_array();
    if (!a) {
      mistyped(*n, key, "an array of tables ([[" + std::string(key) + "]])");
      return;
    }
    for (auto&& el : *a) {
      if (auto* t = el.as_table())
        each(*t);
      else
        mistyped(el, key, "an array of tables ([[" + std::string(key) + "]])");
    }
  }

  void missing(std::string_view key) {
    d_.error(loc(), "missing required field " + squote(key) + " in " + what_);
  }

 private:
  void mistyped(const toml::node& n, std::string_view key, std::string_view expected) {
    d_.error(at(n.source()), "field " + squote(key) + " must be " + std::string(expected));
  }

  const toml::table& t_;
  std::string_view src_;
  const std::string& file_;
  Diags& d_;
  std::string what_;
};

std::optional<toml::table> parse_toml(std::string_view src, const std::string& file, Diags& d) {
  try {
    return toml::parse(src, file);
  } catch (const toml::parse_error& e) {
    d.error({file, static_cast<int>(e.source().begin.line)}, std::string(e.description()));
    return std::nullopt;
  }
}

std::vector<Gotcha> read_gotchas(Reader& r, std::string_view src, const std::string& file,
                                 Diags& d) {
  std::vector<Gotcha> out;
  r.tables("gotcha", [&](const toml::table& t) {
    Reader g(t, src, file, d, "[[gotcha]]", {"level", "text"});
    Gotcha x;
    x.loc = g.loc();
    auto level = g.str("level", true);
    if (auto l = enum_from<Level>(level_names, level))
      x.level = *l;
    else if (!level.empty())
      d.error(g.key_loc("level"), "unknown gotcha level " + squote(level) + " (use ub, warn or perf)");
    x.text = g.str("text", true);
    out.push_back(std::move(x));
  });
  return out;
}

Test read_test(Reader& r, std::string_view src, const std::string& file, Diags& d) {
  Test out;
  out.loc = r.loc();
  auto* t = r.table("test", true);
  if (!t) return out;
  Reader tr(*t, src, file, d, "[test]", {"includes", "global", "prelude", "chain", "skip"});
  out.loc = tr.loc();
  out.includes = tr.strs("includes", false);
  out.global = tr.text("global", false);
  out.prelude = tr.text("prelude", false);
  out.chain = tr.boolean("chain");
  tr.tables("skip", [&](const toml::table& s) {
    Reader sr(s, src, file, d, "a [test] skip", {"compiler", "reason"});
    Skip x;
    x.loc = sr.loc();
    x.compiler = sr.str("compiler", true);
    x.reason = sr.str("reason", false);
    out.skip.push_back(std::move(x));
  });
  return out;
}

Group read_group(const toml::table& t, std::string_view src, const std::string& file, Diags& d,
                 std::string_view what, std::string_view names_key, bool with_since) {
  Group g;
  Reader r(t, src, file, d, std::string(what),
           with_since ? std::initializer_list<std::string_view>{"title", names_key, "since"}
                      : std::initializer_list<std::string_view>{"title", names_key});
  g.loc = r.loc();
  g.title = r.str("title", true);
  g.names = r.strs(names_key, true);
  if (with_since) g.since = r.std_value("since", false);
  return g;
}

std::string read_file(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::string s{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  if (s.starts_with("\xEF\xBB\xBF")) s.erase(0, 3);
  std::erase(s, '\r');  // a CRLF checkout must not change snippets or lint results
  return s;
}

}  // namespace

void parse_entry(std::string_view src, const std::string& file, Data& out, Diags& d) {
  auto tbl = parse_toml(src, file, d);
  if (!tbl) return;
  Reader r(*tbl, src, file, d, "card",
           {"id", "name", "kind", "header", "since", "summary", "url", "parent", "badges",
            "aliases", "tags", "complexity", "see_also", "verified", "support", "signature",
            "member_group", "example", "gotcha", "test"});
  Entry e;
  e.loc = {file, 1};
  e.id = r.str("id", true);
  e.name = r.str("name", true);
  auto kind = r.str("kind", true);
  if (auto k = enum_from<Kind>(kind_names, kind))
    e.kind = *k;
  else if (!kind.empty())
    d.error(r.key_loc("kind"), "unknown kind " + squote(kind));
  e.header = r.str("header", true);
  e.since = r.std_value("since", true);
  e.summary = r.str("summary", true);
  e.url = r.str("url", true);
  e.parent = r.str("parent", false);
  e.badges = r.strs("badges", false);
  e.aliases = r.strs("aliases", false);
  e.tags = r.strs("tags", false);
  e.complexity = r.str("complexity", false);
  e.see_also = r.strs("see_also", false);
  e.verified = r.date("verified");
  if (auto* s = r.table("support", false)) {
    Reader sr(*s, src, file, d, "[support]",
              {"gcc", "clang", "msvc", "apple_clang", "ftm", "ftm_value"});
    e.support.loc = sr.loc();
    e.support.gcc = sr.str("gcc", false);
    e.support.clang = sr.str("clang", false);
    e.support.msvc = sr.str("msvc", false);
    e.support.apple_clang = sr.str("apple_clang", false);
    e.support.ftm = sr.str("ftm", false);
    e.support.ftm_value = sr.str("ftm_value", false);
  }
  r.tables("signature", [&](const toml::table& t) {
    Reader s(t, src, file, d, "[[signature]]", {"text", "note", "since"});
    Signature x;
    x.loc = s.loc();
    x.text = s.str("text", true);
    x.note = s.str("note", false);
    x.since = s.std_value("since", false);
    e.signatures.push_back(std::move(x));
  });
  r.tables("member_group", [&](const toml::table& t) {
    e.members.push_back(read_group(t, src, file, d, "[[member_group]]", "members", true));
  });
  r.tables("example", [&](const toml::table& t) {
    Reader s(t, src, file, d, "[[example]]", {"code", "comment", "std"});
    Snippet x;
    x.loc = s.loc();
    x.code = s.text("code", true);
    x.comment = s.str("comment", false);
    x.std = s.std_value("std", false);
    e.examples.push_back(std::move(x));
  });
  e.gotchas = read_gotchas(r, src, file, d);
  e.test = read_test(r, src, file, d);
  out.entries.push_back(std::move(e));
}

void parse_recipe(std::string_view src, const std::string& file, Data& out, Diags& d) {
  auto tbl = parse_toml(src, file, d);
  if (!tbl) return;
  Reader r(*tbl, src, file, d, "recipe",
           {"id", "title", "summary", "tags", "uses", "variant", "gotcha", "test"});
  Recipe x;
  x.loc = {file, 1};
  x.id = r.str("id", true);
  x.title = r.str("title", true);
  x.summary = r.str("summary", true);
  x.tags = r.strs("tags", true);
  x.uses = r.strs("uses", false);
  r.tables("variant", [&](const toml::table& t) {
    Reader s(t, src, file, d, "[[variant]]", {"title", "since", "code"});
    Snippet v;
    v.loc = s.loc();
    v.title = s.str("title", true);
    v.std = s.std_value("since", true);
    v.code = s.text("code", true);
    x.variants.push_back(std::move(v));
  });
  x.gotchas = read_gotchas(r, src, file, d);
  x.test = read_test(r, src, file, d);
  out.recipes.push_back(std::move(x));
}

void parse_compare(std::string_view src, const std::string& file, Data& out, Diags& d) {
  auto tbl = parse_toml(src, file, d);
  if (!tbl) return;
  Reader r(*tbl, src, file, d, "compare", {"id", "title", "subjects", "tags", "row", "verdict"});
  Compare x;
  x.loc = {file, 1};
  x.id = r.str("id", true);
  x.title = r.str("title", true);
  x.subjects = r.strs("subjects", true);
  x.tags = r.strs("tags", false);
  r.tables("row", [&](const toml::table& t) {
    Reader s(t, src, file, d, "[[row]]", {"label", "values"});
    Row row;
    row.loc = s.loc();
    row.label = s.str("label", true);
    row.values = s.strs("values", true);
    x.rows.push_back(std::move(row));
  });
  r.tables("verdict", [&](const toml::table& t) {
    Reader s(t, src, file, d, "[[verdict]]", {"text"});
    x.verdicts.push_back(s.str("text", true));
  });
  out.compares.push_back(std::move(x));
}

void parse_header(std::string_view src, const std::string& file, Data& out, Diags& d) {
  auto tbl = parse_toml(src, file, d);
  if (!tbl) return;
  Reader r(*tbl, src, file, d, "header file", {"header", "summary", "group"});
  Header h;
  h.loc = {file, 1};
  h.header = r.str("header", true);
  h.summary = r.str("summary", true);
  r.tables("group", [&](const toml::table& t) {
    h.groups.push_back(read_group(t, src, file, d, "[[group]]", "names", false));
  });
  out.headers.push_back(std::move(h));
}

void parse_index(std::string_view tsv, const std::string& file, Data& out, Diags& d) {
  constexpr std::string_view columns = "name\theader\tkind\tsince\turl";
  int line_no = 0;
  std::size_t pos = 0;
  while (pos < tsv.size()) {
    auto end = tsv.find('\n', pos);
    auto line = tsv.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
    pos = end == std::string_view::npos ? tsv.size() : end + 1;
    ++line_no;
    const Loc at{file, line_no};
    if (line_no == 1) {
      if (line != columns) d.error(at, "the first line must be the column names: name header kind since url");
      continue;
    }
    std::vector<std::string_view> f;
    for (std::size_t s = 0;;) {
      auto tab = line.find('\t', s);
      f.push_back(line.substr(s, tab == std::string_view::npos ? std::string_view::npos : tab - s));
      if (tab == std::string_view::npos) break;
      s = tab + 1;
    }
    if (f.size() != 5) {
      d.error(at, "expected 5 tab-separated columns, found " + std::to_string(f.size()));
      continue;
    }
    IndexName n;
    n.loc = at;
    n.name = f[0];
    n.header = f[1];
    n.url = f[4];
    if (n.name.empty() || n.header.empty() || n.url.empty())
      d.error(at, "name, header and url must not be empty");
    if (!f[2].empty()) {
      if (auto k = enum_from<Kind>(kind_names, f[2]))
        n.kind = *k;
      else
        d.error(at, "unknown kind " + squote(f[2]));
    }
    if (!f[3].empty()) n.since = parse_std(f[3], at, "since", d);
    out.index.push_back(std::move(n));
  }
  if (line_no == 0) d.error({file, 1}, "empty index; the first line must be the column names");
}

void sort_tables(Data& data) {
  auto by = [](auto member) {
    return [member](const auto& a, const auto& b) { return a.*member < b.*member; };
  };
  std::ranges::sort(data.entries, by(&Entry::id));
  std::ranges::sort(data.recipes, by(&Recipe::id));
  std::ranges::sort(data.compares, by(&Compare::id));
  std::ranges::sort(data.headers, by(&Header::header));
  std::ranges::sort(data.index, [](const IndexName& a, const IndexName& b) {
    return std::tie(a.name, a.header, a.url) < std::tie(b.name, b.header, b.url);
  });
}

Data load(const std::filesystem::path& data_dir, Diags& d) {
  namespace fs = std::filesystem;
  Data data;
  const fs::path dir = fs::absolute(data_dir).lexically_normal();
  data.root = dir.parent_path();
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) {
    d.error({data_dir.generic_string(), 0}, "not a directory");
    return data;
  }
  std::vector<fs::path> files;
  for (auto& f : fs::recursive_directory_iterator(dir))
    if (f.is_regular_file()) files.push_back(f.path());
  std::ranges::sort(files);

  bool have_index = false;
  for (const auto& p : files) {
    const std::string shown = p.lexically_relative(data.root).generic_string();
    const fs::path rel = p.lexically_relative(dir);
    const std::string top = rel.begin()->generic_string();
    if (p.extension() == ".toml") {
      const std::string src = read_file(p);
      if (top == "entries")
        parse_entry(src, shown, data, d);
      else if (top == "recipes")
        parse_recipe(src, shown, data, d);
      else if (top == "compare")
        parse_compare(src, shown, data, d);
      else if (top == "headers")
        parse_header(src, shown, data, d);
      else
        d.error({shown, 1}, "TOML files belong in entries/, recipes/, compare/ or headers/");
    } else if (rel.generic_string() == "index/names.tsv") {
      parse_index(read_file(p), shown, data, d);
      have_index = true;
    }
  }
  if (!have_index) d.error({(dir / "index/names.tsv").generic_string(), 0}, "missing name index");
  sort_tables(data);
  return data;
}

}  // namespace cxxman::gen
