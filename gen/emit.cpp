// Writes data.cpp (PLAN §7.3) and the snippet translation units (PLAN §5.5).
#include "schema.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
#include <tuple>

namespace cxxman::gen {

std::string normalize_key(std::string_view name) {
  auto first = name.find_first_not_of(" \t");
  if (first == std::string_view::npos) return {};
  name = name.substr(first, name.find_last_not_of(" \t") - first + 1);
  if (name.starts_with("std::")) name.remove_prefix(5);
  return std::string(name);
}

std::optional<std::string> changelog_section(std::string_view md, std::string_view version) {
  const std::string heading = "## " + std::string(version);
  std::string body;
  bool inside = false;
  std::size_t pos = 0;
  while (pos < md.size()) {
    auto end = md.find('\n', pos);
    auto line = md.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
    pos = end == std::string_view::npos ? md.size() : end + 1;
    if (line.starts_with("## ")) {
      if (inside) break;
      auto rest = line.substr(heading.size() > line.size() ? line.size() : heading.size());
      inside = line.starts_with(heading) && (rest.empty() || rest[0] == ' ');
      continue;
    }
    if (inside) body.append(line).push_back('\n');
  }
  if (!inside && body.empty()) return std::nullopt;
  auto first = body.find_first_not_of('\n');
  if (first == std::string::npos) return std::string{};
  return body.substr(first, body.find_last_not_of('\n') - first + 1);
}

namespace {

constexpr std::string_view std_enum(Std s) {
  constexpr std::string_view names[] = {"Cpp98", "Cpp11", "Cpp14", "Cpp17", "Cpp20",
                                        "Cpp23", "Cpp26", "Cpp29", "Unknown"};
  return names[static_cast<std::size_t>(s)];
}

constexpr std::string_view kind_enum(Kind k) {
  constexpr std::string_view names[] = {"Function", "Algorithm", "Class",  "Member",
                                        "Alias",    "Trait",     "Concept", "Constant",
                                        "Macro",    "Object",    "Enum",    "Unknown"};
  return names[static_cast<std::size_t>(k)];
}

constexpr std::string_view level_enum(Level l) {
  constexpr std::string_view names[] = {"Ub", "Warn", "Perf"};
  return names[static_cast<std::size_t>(l)];
}

// A std::string_view literal. Long text is split into short adjacent tokens
// (MSVC C2026 limits one token, not the concatenation).
std::string lit(std::string_view s) {
  std::string out = "\"";
  std::size_t run = 0;
  for (unsigned char c : s) {
    if (run++ == 2000) {
      out += "\" \"";
      run = 1;
    }
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20 || c >= 0x7F) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\%03o", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out + "\"sv";
}

std::string fold(std::string_view s) {
  std::string out(s);
  for (auto& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

// Collects helper arrays; each returns the expression for a span member.
class Tables {
 public:
  std::string strs(const std::vector<std::string>& v) {
    std::vector<std::string> items;
    for (const auto& s : v) items.push_back(lit(s));
    return array("std::string_view", items);
  }

  std::string array(std::string_view type, const std::vector<std::string>& items) {
    if (items.empty()) return "{}";
    std::string name = "t" + std::to_string(next_++);
    out_ += "constexpr " + std::string(type) + " " + name + "[] = {\n";
    for (const auto& i : items) out_ += "    " + i + ",\n";
    out_ += "};\n";
    return name;
  }

  std::string signatures(const std::vector<Signature>& v) {
    std::vector<std::string> items;
    for (const auto& s : v)
      items.push_back("{.text = " + lit(s.text) + ", .note = " + lit(s.note) +
                      ", .since = Std::" + std::string(std_enum(s.since)) + "}");
    return array("Signature", items);
  }

  std::string snippets(const std::vector<Snippet>& v) {
    std::vector<std::string> items;
    for (const auto& s : v)
      items.push_back("{.title = " + lit(s.title) + ", .code = " + lit(s.code.value) +
                      ", .comment = " + lit(s.comment) + ", .std = Std::" +
                      std::string(std_enum(s.std)) + "}");
    return array("Snippet", items);
  }

  std::string gotchas(const std::vector<Gotcha>& v) {
    std::vector<std::string> items;
    for (const auto& g : v)
      items.push_back("{.level = Level::" + std::string(level_enum(g.level)) +
                      ", .text = " + lit(g.text) + "}");
    return array("Gotcha", items);
  }

  std::string groups(const std::vector<Group>& v) {
    std::vector<std::string> items;
    for (const auto& g : v)
      items.push_back("{.title = " + lit(g.title) + ", .names = " + strs(g.names) +
                      ", .since = Std::" + std::string(std_enum(g.since)) + "}");
    return array("Group", items);
  }

  const std::string& text() const { return out_; }

 private:
  std::string out_;
  int next_ = 0;
};

std::string accessor(std::string_view type, std::string_view fn, std::string_view table) {
  return "std::span<const " + std::string(type) + "> " + std::string(fn) + "() noexcept { return " +
         (table == "{}" ? std::string("{}") : std::string(table)) + "; }\n";
}

}  // namespace

std::string emit_data_cpp(const Data& data, std::string_view changelog) {
  Tables t;

  std::map<std::string, std::int32_t> card_of;  // normalized id/alias -> entry index
  std::vector<std::string> entries;
  for (std::size_t i = 0; i < data.entries.size(); ++i) {
    const Entry& e = data.entries[i];
    card_of.emplace(normalize_key(e.id), static_cast<std::int32_t>(i));
    for (const auto& a : e.aliases) card_of.emplace(normalize_key(a), static_cast<std::int32_t>(i));
    const Support& s = e.support;
    entries.push_back(
        "{.id = " + lit(e.id) + ", .name = " + lit(e.name) + ", .header = " + lit(e.header) +
        ", .summary = " + lit(e.summary) + ", .complexity = " + lit(e.complexity) +
        ", .url = " + lit(e.url) + ", .parent = " + lit(e.parent) +
        ", .verified = " + lit(e.verified) + ",\n     .kind = Kind::" +
        std::string(kind_enum(e.kind)) + ", .since = Std::" + std::string(std_enum(e.since)) +
        ",\n     .support = {.gcc = " + lit(s.gcc) + ", .clang = " + lit(s.clang) +
        ", .msvc = " + lit(s.msvc) + ", .apple_clang = " + lit(s.apple_clang) +
        ", .ftm = " + lit(s.ftm) + ", .ftm_value = " + lit(s.ftm_value) + "}" +
        ",\n     .badges = " + t.strs(e.badges) + ", .aliases = " + t.strs(e.aliases) +
        ", .tags = " + t.strs(e.tags) + ", .see_also = " + t.strs(e.see_also) +
        ",\n     .signatures = " + t.signatures(e.signatures) +
        ", .members = " + t.groups(e.members) + ", .examples = " + t.snippets(e.examples) +
        ", .gotchas = " + t.gotchas(e.gotchas) + "}");
  }
  const std::string entries_table = t.array("Entry", entries);

  std::vector<std::string> recipes;
  for (const auto& r : data.recipes)
    recipes.push_back("{.id = " + lit(r.id) + ", .title = " + lit(r.title) +
                      ", .summary = " + lit(r.summary) + ", .tags = " + t.strs(r.tags) +
                      ", .uses = " + t.strs(r.uses) + ", .variants = " + t.snippets(r.variants) +
                      ", .gotchas = " + t.gotchas(r.gotchas) + "}");
  const std::string recipes_table = t.array("Recipe", recipes);

  std::vector<std::string> compares;
  for (const auto& c : data.compares) {
    std::vector<std::string> rows;
    for (const auto& r : c.rows)
      rows.push_back("{.label = " + lit(r.label) + ", .values = " + t.strs(r.values) + "}");
    const std::string rows_table = t.array("Row", rows);
    compares.push_back("{.id = " + lit(c.id) + ", .title = " + lit(c.title) +
                       ", .subjects = " + t.strs(c.subjects) + ", .tags = " + t.strs(c.tags) +
                       ", .verdicts = " + t.strs(c.verdicts) + ", .rows = " + rows_table + "}");
  }
  const std::string compares_table = t.array("Compare", compares);

  std::vector<std::string> headers;
  for (const auto& h : data.headers)
    headers.push_back("{.header = " + lit(h.header) + ", .summary = " + lit(h.summary) +
                      ", .groups = " + t.groups(h.groups) + "}");
  const std::string headers_table = t.array("Header", headers);

  std::vector<std::string> index;
  for (const auto& n : data.index) {
    auto it = card_of.find(normalize_key(n.name));
    index.push_back("{" + lit(n.name) + ", " + lit(n.header) + ", " + lit(n.url) + ", Kind::" +
                    std::string(kind_enum(n.kind)) + ", Std::" + std::string(std_enum(n.since)) +
                    ", " + std::to_string(it == card_of.end() ? -1 : it->second) + "}");
  }
  const std::string index_table = t.array("IndexName", index);

  using KeyRow = std::tuple<std::string, std::string, int, std::int32_t>;  // folded, key, target, index
  std::set<KeyRow> keys;
  auto add_key = [&](std::string_view name, int target, std::size_t i) {
    auto k = normalize_key(name);
    keys.emplace(fold(k), k, target, static_cast<std::int32_t>(i));
  };
  for (std::size_t i = 0; i < data.entries.size(); ++i) {
    add_key(data.entries[i].id, 0, i);
    for (const auto& a : data.entries[i].aliases) add_key(a, 0, i);
  }
  for (std::size_t i = 0; i < data.recipes.size(); ++i) add_key(data.recipes[i].id, 1, i);
  for (std::size_t i = 0; i < data.compares.size(); ++i) add_key(data.compares[i].id, 2, i);
  for (std::size_t i = 0; i < data.index.size(); ++i) add_key(data.index[i].name, 3, i);
  constexpr std::string_view targets[] = {"Entry", "Recipe", "Compare", "Index"};
  std::vector<std::string> key_items;
  for (const auto& [folded, key, target, i] : keys)
    key_items.push_back("{" + lit(key) + ", " + lit(folded) + ", Target::" +
                        std::string(targets[target]) + ", " + std::to_string(i) + "}");
  const std::string keys_table = t.array("Key", key_items);

  std::string out =
      "// Generated by cxxman-gen from data/. Do not edit.\n"
      "#include \"model/model.hpp\"\n\n"
      "namespace cxxman::data {\n"
      "namespace {\n"
      "using namespace std::string_view_literals;\n\n";
  out += t.text();
  out += "\nconstexpr std::string_view changelog_text = " + lit(changelog) + ";\n";
  out += "}  // namespace\n\n";
  out += accessor("Entry", "entries", entries_table);
  out += accessor("Recipe", "recipes", recipes_table);
  out += accessor("Compare", "compares", compares_table);
  out += accessor("Header", "headers", headers_table);
  out += accessor("IndexName", "index_names", index_table);
  out += accessor("Key", "keys", keys_table);
  out += "std::string_view changelog() noexcept { return changelog_text; }\n\n";
  out += "}  // namespace cxxman::data\n";
  return out;
}

namespace {

std::string stem(std::string_view prefix, std::string_view id) {
  std::string out(prefix);
  for (char c : id)
    out += (std::isalnum(static_cast<unsigned char>(c)) || c == '-') ? c : '_';
  return out;
}

// Appends a block of TOML text with #line so compiler errors point at the TOML.
void block(std::string& src, const std::string& path, const Text& t) {
  if (t.value.empty()) return;
  // A one-line TOML string with \n escapes: every line maps to the same TOML
  // line. A directive inside a raw string literal would become its content.
  const bool per_line = !t.block && t.value.find("R\"") == std::string::npos;
  std::size_t pos = 0;
  for (int k = 0; pos <= t.value.size(); ++k) {
    auto end = t.value.find('\n', pos);
    if (end == std::string::npos) end = t.value.size();
    if (k == 0 || per_line) src += "#line " + std::to_string(t.loc.line) + " \"" + path + "\"\n";
    src.append(t.value, pos, end - pos).push_back('\n');
    pos = end + 1;
  }
}

SnippetFile snippet(const Data& data, std::string name, const std::string& file, const Test& t,
                    const std::vector<Snippet>& snips, std::size_t i, Std base) {
  SnippetFile f;
  f.name = std::move(name);
  const std::size_t first = t.chain ? 0 : i;
  f.std = base == Std::Unknown ? snippet_floor : std::max(snippet_floor, base);
  for (std::size_t j = first; j <= i; ++j)
    if (snips[j].std != Std::Unknown) f.std = std::max(f.std, snips[j].std);
  for (const auto& s : t.skip) f.skip.push_back(s.compiler);

  const std::string path = data.root.empty() ? file : (data.root / file).generic_string();
  std::string& src = f.source;
  src = "// Generated by cxxman-gen from " + file + ". Do not edit.\n";
  for (const auto& inc : t.includes) src += "#include <" + inc + ">\n";
  block(src, path, t.global);
  src += "int main() {\n";
  block(src, path, t.prelude);
  for (std::size_t j = first; j <= i; ++j) block(src, path, snips[j].code);
  src += "}\n";
  return f;
}

}  // namespace

std::vector<SnippetFile> make_snippets(const Data& data) {
  std::vector<SnippetFile> out;
  std::set<std::string> names;
  auto add = [&](SnippetFile f) {
    std::string base = f.name;
    for (int n = 2; !names.insert(f.name).second; ++n) f.name = base + "_" + std::to_string(n);
    out.push_back(std::move(f));
  };
  for (const auto& e : data.entries)
    for (std::size_t i = 0; i < e.examples.size(); ++i)
      add(snippet(data, stem("entry-", e.id) + "-" + std::to_string(i), e.loc.file, e.test,
                  e.examples, i, e.since));
  for (const auto& r : data.recipes)
    for (std::size_t i = 0; i < r.variants.size(); ++i)
      add(snippet(data, stem("recipe-", r.id) + "-" + std::to_string(i), r.loc.file, r.test,
                  r.variants, i, Std::Unknown));
  return out;
}

std::string emit_manifest(const std::vector<SnippetFile>& snippets) {
  constexpr int numbers[] = {98, 11, 14, 17, 20, 23, 26, 29};
  std::string out = "# Generated by cxxman-gen. Do not edit.\n";
  for (const auto& s : snippets) {
    std::string skip;
    for (const auto& c : s.skip) skip += (skip.empty() ? "" : ";") + c;
    out += "cxxman_snippet(" + s.name + " " +
           std::to_string(numbers[static_cast<std::size_t>(s.std)]) + " \"" + skip + "\")\n";
  }
  return out;
}

}  // namespace cxxman::gen
