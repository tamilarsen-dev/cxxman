// The content linter (PLAN §5.7): budgets, cross-references and the rules a
// schema cannot express. Errors fail the build; warnings are printed.
#include "schema.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <tuple>

namespace cxxman::gen {

namespace {

std::string q(std::string_view s) { return "'" + std::string(s) + "'"; }

class Linter {
 public:
  Linter(const Data& data, std::string_view today, Diags& d) : data_(data), today_(today), d_(d) {}

  void run() {
    index();
    card_keys();
    std::set<std::string> seen;
    for (const auto& e : data_.entries) entry(e);
    for (const auto& r : data_.recipes) {
      if (!seen.insert("recipe " + r.id).second) d_.error(r.loc, "duplicate recipe id " + q(r.id));
      recipe(r);
    }
    for (const auto& c : data_.compares) {
      if (!seen.insert("compare " + c.id).second) d_.error(c.loc, "duplicate compare id " + q(c.id));
      compare(c);
    }
    for (const auto& h : data_.headers) {
      if (!seen.insert("header " + h.header).second)
        d_.error(h.loc, "a second header file for <" + h.header + ">");
      header(h);
    }
  }

 private:
  void index() {
    std::set<std::tuple<std::string, std::string, std::string>> rows;
    for (const auto& n : data_.index) {
      if (!rows.emplace(n.name, n.header, n.url).second)
        d_.error(n.loc, "duplicate index row for " + q(n.name));
      headers_.insert(n.header);
      names_.insert(normalize_key(n.name));
    }
  }

  void card_keys() {
    for (const auto& e : data_.entries) {
      std::vector<std::string> keys{e.id};
      keys.insert(keys.end(), e.aliases.begin(), e.aliases.end());
      for (const auto& k : keys) {
        auto [it, fresh] = cards_.emplace(normalize_key(k), &e);
        if (!fresh && it->second != &e)
          d_.error(e.loc, q(k) + " is already the id or an alias of card " + q(it->second->id));
      }
    }
  }

  bool resolves(const std::string& name) const {
    auto k = normalize_key(name);
    return cards_.contains(k) || names_.contains(k);
  }

  void refs(const Loc& at, std::string_view field, const std::vector<std::string>& names) {
    for (const auto& n : names)
      if (!resolves(n))
        d_.error(at, std::string(field) + " names " + q(n) +
                         ", which is neither a card nor in index/names.tsv");
  }

  void len(const Loc& at, std::string_view what, std::string_view s, std::size_t max) {
    auto n = display_length(s);
    if (n > max)
      d_.error(at, std::string(what) + " is " + std::to_string(n) + " characters; the budget is " +
                       std::to_string(max));
  }

  void required(const Loc& at, std::string_view what, std::string_view s) {
    if (s.empty()) d_.error(at, std::string(what) + " must not be empty");
  }

  void count(const Loc& at, std::string_view what, std::size_t n, std::size_t min,
             std::size_t max) {
    if (n < min || n > max)
      d_.error(at, "needs " + (min == max ? std::to_string(min)
                                          : std::to_string(min) + "-" + std::to_string(max)) +
                       " " + std::string(what) + ", found " + std::to_string(n));
  }

  void summary(const Loc& at, std::string_view s) {
    required(at, "summary", s);
    len(at, "summary", s, budget::summary);
    if (!s.empty() && s.back() != '.') d_.error(at, "summary must end with '.'");
  }

  void header_known(const Loc& at, const std::string& h) {
    if (!h.empty() && !headers_.contains(h))
      d_.error(at, "header <" + h + "> is not in index/names.tsv");
  }

  // Code shown on a card: no tabs or trailing whitespace, width and line budgets.
  void code(const Text& t, std::string_view what, std::size_t max_lines, bool shown) {
    std::size_t lines = 0;
    std::size_t pos = 0;
    while (pos <= t.value.size()) {
      auto end = t.value.find('\n', pos);
      if (end == std::string::npos) end = t.value.size();
      std::string_view line(t.value.data() + pos, end - pos);
      Loc at = t.loc;
      if (t.block) at.line += static_cast<int>(lines);
      ++lines;
      if (line.find('\t') != std::string_view::npos)
        d_.error(at, std::string(what) + " contains a tab; indent with spaces");
      if (!line.empty() && (line.back() == ' '))
        d_.error(at, std::string(what) + " has trailing whitespace");
      if (shown) len(at, std::string(what) + " line", line, budget::code_width);
      pos = end + 1;
    }
    if (shown && lines > max_lines)
      d_.error(t.loc, std::string(what) + " has " + std::to_string(lines) +
                          " lines; the budget is " + std::to_string(max_lines));
  }

  void gotchas(const std::vector<Gotcha>& gs, const Loc& at, bool warn_if_none) {
    if (gs.size() > budget::gotchas) count(at, "gotchas", gs.size(), 0, budget::gotchas);
    if (gs.empty() && warn_if_none) d_.warn(at, "no gotchas");
    for (const auto& g : gs) {
      required(g.loc, "gotcha text", g.text);
      len(g.loc, "gotcha text", g.text, budget::gotcha);
    }
  }

  void tags(const std::vector<std::string>& t, const Loc& at) {
    if (t.empty()) d_.warn(at, "no tags; search will not find this by task");
    if (t.size() > budget::tags) count(at, "tags", t.size(), 0, budget::tags);
  }

  void test(const Test& t) {
    code(t.global, "test.global", 0, false);
    code(t.prelude, "test.prelude", 0, false);
    for (const auto& s : t.skip) {
      if (s.compiler != "gcc" && s.compiler != "clang" && s.compiler != "msvc")
        d_.error(s.loc, "skip compiler must be gcc, clang or msvc, not " + q(s.compiler));
      if (s.reason.empty()) d_.error(s.loc, "skip needs a reason");
    }
  }

  static bool version_like(std::string_view s) {
    if (s.empty() || s.front() == '.' || s.back() == '.') return false;
    return std::ranges::all_of(s, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) || c == '.'; }) &&
           s.find("..") == std::string_view::npos;
  }

  void support(const Support& s) {
    using P = std::pair<const char*, const std::string*>;
    for (auto [name, v] : {P{"gcc", &s.gcc}, P{"clang", &s.clang}, P{"msvc", &s.msvc},
                           P{"apple_clang", &s.apple_clang}})
      if (!v->empty() && !version_like(*v))
        d_.error(s.loc, std::string("[support] ") + name + " " + q(*v) + " is not a version number");
    if (!s.ftm.empty() &&
        (!s.ftm.starts_with("__cpp_") ||
         !std::ranges::all_of(s.ftm, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; })))
      d_.error(s.loc, "[support] ftm " + q(s.ftm) + " is not a feature-test macro");
    if (!s.ftm_value.empty() &&
        (s.ftm_value.size() != 7 || s.ftm_value.back() != 'L' ||
         !std::all_of(s.ftm_value.begin(), s.ftm_value.end() - 1,
                      [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })))
      d_.error(s.loc, "[support] ftm_value " + q(s.ftm_value) + " must look like 201907L");
  }

  void entry(const Entry& e) {
    const Loc& at = e.loc;
    required(at, "id", e.id);
    required(at, "name", e.name);
    if (e.id.starts_with("std::")) d_.error(at, "id must not start with std::");
    header_known(at, e.header);
    summary(at, e.summary);
    required(at, "url", e.url);
    if (e.url.find("://") != std::string::npos || e.url.starts_with("/"))
      d_.error(at, "url is the path after " + std::string(cppreference_base));

    if (e.kind == Kind::Member) {
      if (e.parent.empty()) {
        d_.error(at, "a member card needs 'parent'");
      } else {
        auto it = std::ranges::find(data_.entries, e.parent, &Entry::id);
        if (it == data_.entries.end() || it->kind != Kind::Class)
          d_.error(at, "parent " + q(e.parent) + " is not a class card");
        if (!e.id.starts_with(e.parent + "::"))
          d_.error(at, "a member id must be " + e.parent + "::<name>");
      }
    } else if (!e.parent.empty()) {
      d_.error(at, "'parent' is only for kind = \"member\"");
    }

    if (e.badges.size() > budget::badges) count(at, "badges", e.badges.size(), 0, budget::badges);
    tags(e.tags, at);
    len(at, "complexity", e.complexity, budget::complexity);
    if (e.see_also.size() > budget::see_also)
      count(at, "see_also names", e.see_also.size(), 0, budget::see_also);
    refs(at, "see_also", e.see_also);

    if (e.verified.empty())
      d_.warn(at, "not verified against upstream yet");
    else if (e.verified > today_)
      d_.error(at, "verified date " + e.verified + " is in the future");
    support(e.support);

    count(at, "[[signature]] tables", e.signatures.size(), 1, budget::signatures);
    for (const auto& s : e.signatures) {
      required(s.loc, "signature text", s.text);
      len(s.loc, "signature text", s.text, budget::signature);
      len(s.loc, "signature note", s.note, budget::signature_note);
    }

    if (e.kind == Kind::Class) {
      if (e.members.empty()) d_.warn(at, "a class card without [[member_group]]");
    } else if (!e.members.empty()) {
      d_.error(e.members.front().loc, "[[member_group]] is only for class cards");
    }
    if (e.members.size() > budget::member_groups)
      count(at, "[[member_group]] tables", e.members.size(), 0, budget::member_groups);
    for (const auto& g : e.members) {
      required(g.loc, "member group title", g.title);
      if (g.names.empty()) d_.error(g.loc, "member group has no members");
    }

    count(at, "[[example]] tables", e.examples.size(), 1, budget::examples);
    for (const auto& x : e.examples) {
      required(x.code.loc, "example code", x.code.value);
      code(x.code, "example code", budget::example_lines, true);
      len(x.loc, "example comment", x.comment, budget::example_comment);
    }
    gotchas(e.gotchas, at, true);
    test(e.test);
  }

  void recipe(const Recipe& r) {
    const Loc& at = r.loc;
    required(at, "id", r.id);
    if (!std::ranges::all_of(r.id, [](char c) {
          return std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '-';
        }))
      d_.error(at, "recipe id " + q(r.id) + " must be kebab-case");
    required(at, "title", r.title);
    len(at, "title", r.title, budget::title);
    summary(at, r.summary);
    tags(r.tags, at);
    refs(at, "uses", r.uses);
    count(at, "[[variant]] tables", r.variants.size(), 1, budget::variants);
    for (const auto& v : r.variants) {
      required(v.loc, "variant title", v.title);
      len(v.loc, "variant title", v.title, budget::variant_title);
      required(v.code.loc, "variant code", v.code.value);
      code(v.code, "variant code", budget::variant_lines, true);
    }
    gotchas(r.gotchas, at, false);
    test(r.test);
  }

  void compare(const Compare& c) {
    const Loc& at = c.loc;
    required(at, "id", c.id);
    required(at, "title", c.title);
    len(at, "title", c.title, budget::title);
    count(at, "subjects", c.subjects.size(), budget::subjects_min, budget::subjects_max);
    refs(at, "subjects", c.subjects);
    tags(c.tags, at);
    count(at, "[[row]] tables", c.rows.size(), budget::rows_min, budget::rows_max);
    for (const auto& r : c.rows) {
      required(r.loc, "row label", r.label);
      len(r.loc, "row label", r.label, budget::row_label);
      if (r.values.size() != c.subjects.size())
        d_.error(r.loc, "row " + q(r.label) + " has " + std::to_string(r.values.size()) +
                            " values for " + std::to_string(c.subjects.size()) + " subjects");
      for (const auto& v : r.values) len(r.loc, "row value", v, budget::row_value);
    }
    count(at, "[[verdict]] tables", c.verdicts.size(), 1, budget::verdicts);
    for (const auto& v : c.verdicts) len(at, "verdict", v, budget::verdict);
  }

  void header(const Header& h) {
    header_known(h.loc, h.header);
    summary(h.loc, h.summary);
    for (const auto& g : h.groups) {
      required(g.loc, "group title", g.title);
      refs(g.loc, "group", g.names);
    }
  }

  const Data& data_;
  std::string today_;
  Diags& d_;
  std::set<std::string> headers_;
  std::set<std::string> names_;                  // normalized index names
  std::map<std::string, const Entry*> cards_;    // normalized id/alias -> card
};

}  // namespace

void lint(const Data& data, std::string_view today, Diags& d) { Linter(data, today, d).run(); }

}  // namespace cxxman::gen
