#include "render/card.hpp"

#include "render/highlight.hpp"
#include "render/style.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace cxxman::render {

namespace {

constexpr std::size_t indent = 2;       // signatures, examples, gotchas
constexpr std::size_t note_indent = 6;  // a signature note on its own line
constexpr std::size_t comment_gap = 3;  // code to aligned "// comment"
constexpr std::size_t code_hang = 4;    // extra indent of a wrapped code line
constexpr std::size_t max_index_rows = 8;

std::string sep(const Options& o) { return " " + std::string(sym(Sym::Dot, o.ascii)) + " "; }

void blank(Block& out) {
  if (!out.empty() && !out.back().empty()) out.emplace_back();
}

void push(Block& out, Block lines) {
  for (auto& l : lines) out.push_back(std::move(l));
}

bool newer(Std s, const Options& o) { return o.std != Std::Unknown && s != Std::Unknown && s > o.std; }

// `text` in badge, or "⚑ text" in newer when `s` is newer than --std.
Line std_badge(std::string_view text, Std s, const Options& o) {
  Line l;
  if (newer(s, o))
    l.add(std::string(sym(Sym::Newer, o.ascii)) + " " + std::string(text), Role::Newer);
  else
    l.add(text, Role::Badge);
  return l;
}

// What --strict hides: how many items, and the oldest standard they need.
struct Hidden {
  std::size_t count = 0;
  Std need = Std::Unknown;

  bool hides(Std s, const Options& o) {
    if (!o.strict || !newer(s, o)) return false;
    ++count;
    need = std::min(need, s);  // Unknown sorts last
    return true;
  }
};

// The parts of a card that --std --strict leaves.
struct Parts {
  std::vector<std::pair<std::string_view, Std>> badges;
  std::vector<Signature> signatures;
  std::vector<Group> members;
  std::vector<Snippet> examples;
  Hidden hidden;
};

Parts parts_of(const Entry& e, const Options& o) {
  Parts p;
  for (auto b : e.badges)
    if (const Std s = std_of_badge(b); !p.hidden.hides(s, o)) p.badges.emplace_back(b, s);
  for (const auto& s : e.signatures)
    if (!p.hidden.hides(s.since, o)) p.signatures.push_back(s);
  for (const auto& g : e.members)
    if (!p.hidden.hides(g.since, o)) p.members.push_back(g);
  for (const auto& x : e.examples)
    if (!p.hidden.hides(x.std, o)) p.examples.push_back(x);
  return p;
}

// "3 items need C++20+ (hidden by --std 17 --strict)".
void hidden_line(Block& out, const Hidden& h, const Options& o) {
  if (h.count == 0) return;
  const std::string text = std::to_string(h.count) + (h.count == 1 ? " item needs " : " items need ") +
                           std::string(to_string(h.need)) + "+ (hidden by --std " +
                           std::string(to_string(o.std).substr(3)) + " --strict)";
  blank(out);
  push(out, flow(Line{}, words(text, Role::Badge), o.width, 0));
}

// "std::vector<T>" for a class whose first signature is "vector<T>".
std::string title_text(const Entry& e) {
  if (e.kind == Kind::Class && !e.signatures.empty()) {
    const std::string_view s = e.signatures.front().text;
    if (s.size() > e.id.size() && s.starts_with(e.id) && s[e.id.size()] == '<')
      return std::string(e.name) + std::string(s.substr(e.id.size()));
  }
  return std::string(e.name);
}

// Title on the left, badges right-aligned; on a second line if they do not fit.
void title_block(Block& out, Line title, const Line& badges, std::size_t width) {
  if (badges.empty() || put_right(title, badges, width)) {
    out.push_back(std::move(title));
    return;
  }
  out.push_back(std::move(title));
  Line second;
  put_right(second, badges, width, 0);
  out.push_back(std::move(second));
}

// Title, header and badges.
void head(Block& out, const Entry& e, const Parts& p, const Options& o) {
  Line title;
  title.add(title_text(e), Role::Title, reference_url(e.url)).add("  ");
  title.add("<" + std::string(e.header) + ">", Role::Header, header_url(e.header));
  Line badges;
  if (e.since != Std::Unknown) badges.append(std_badge(to_string(e.since), e.since, o));
  for (auto [b, s] : p.badges) {
    if (!badges.empty()) badges.add(sep(o), Role::Badge);
    badges.append(std_badge(b, s, o));
  }
  title_block(out, std::move(title), badges, o.width);
}

void signatures(Block& out, std::span<const Signature> sigs, const Options& o) {
  std::size_t text_w = 0;
  std::vector<Line> notes;
  for (const auto& s : sigs) {
    text_w = std::max(text_w, display_width(s.text));
    Line n;
    if (s.since != Std::Unknown) n.append(std_badge(to_string(s.since), s.since, o));
    if (!s.note.empty()) {
      if (!n.empty()) n.add(sep(o), Role::Badge);
      n.add(s.note, Role::Note);
    }
    notes.push_back(std::move(n));
  }
  const std::size_t col = indent + text_w + comment_gap;
  const bool aligned = std::ranges::all_of(notes, [&](const Line& n) {
    return n.empty() || col + n.width() <= o.width;
  });
  for (std::size_t i = 0; i < sigs.size(); ++i) {
    Line l = spaces(indent);
    l.add(sigs[i].text, Role::Sig);
    const Line& n = notes[i];
    if (n.empty()) {
      out.push_back(std::move(l));
    } else if (aligned) {
      l.pad_to(col).append(n);
      out.push_back(std::move(l));
    } else {
      out.push_back(std::move(l));
      // Re-split the note so it wraps; the badge part stays one item.
      std::vector<Line> items;
      const auto& s = sigs[i];
      if (s.since != Std::Unknown) {
        Line badge = std_badge(to_string(s.since), s.since, o);
        if (!s.note.empty()) badge.add(" " + std::string(sym(Sym::Dot, o.ascii)), Role::Badge);
        items.push_back(std::move(badge));
      }
      for (auto& w : words(s.note, Role::Note)) items.push_back(std::move(w));
      push(out, flow(spaces(note_indent), items, o.width, note_indent));
    }
  }
}

void members(Block& out, const Catalog& cat, const Entry& e, std::span<const Group> groups,
             const Options& o) {
  std::size_t label_w = 0;
  for (const auto& g : groups) label_w = std::max(label_w, display_width(g.title));
  const std::size_t col = indent + label_w + 2;
  for (const auto& g : groups) {
    Line prefix = spaces(indent);
    prefix.add(g.title).pad_to(col);
    std::vector<Line> items;
    for (std::string_view name : g.names) {
      // A member with its own card is a link: "vector::erase".
      std::string url = card_url(cat, std::string(e.id) + "::" + std::string(name));
      if (url.empty() && name.starts_with("std::")) url = card_url(cat, name);
      items.emplace_back().add(name, url.empty() ? Role::Plain : Role::Link, url);
    }
    Block lines = flow(prefix, items, o.width, col, 2);
    if (g.since != Std::Unknown) {
      const Line badge = std_badge(to_string(g.since), g.since, o);
      if (!put_right(lines.back(), badge, o.width)) {
        Line extra;
        put_right(extra, badge, o.width, 0);
        lines.push_back(std::move(extra));
      }
    }
    push(out, std::move(lines));
  }
}

// Code lines of one example, highlighted; with `wrap`, lines wider than the
// card wrap at a space with a deeper indent.
Block code_lines(std::string_view code, const Options& o, bool wrap) {
  const auto roles = highlight(code);
  Block out;
  std::size_t start = 0;
  while (start <= code.size()) {
    auto end = code.find('\n', start);
    if (end == std::string_view::npos) end = code.size();
    std::size_t from = start;
    std::size_t lead = indent;
    while (true) {
      std::string_view rest = code.substr(from, end - from);
      std::size_t take = rest.size();
      if (wrap && lead + display_width(rest) > o.width) {
        const auto avail = o.width > lead ? o.width - lead : 1;
        const auto space = rest.substr(0, avail + 1).rfind(' ');
        take = space != std::string_view::npos && space > 0 ? space : std::min(avail, rest.size());
      }
      Line l = spaces(lead);
      for (std::size_t i = from; i < from + take;) {
        std::size_t j = i;
        while (j < from + take && roles[j] == roles[i]) ++j;
        l.add(code.substr(i, j - i), roles[i]);
        i = j;
      }
      out.push_back(std::move(l));
      from += take;
      while (from < end && code[from] == ' ') ++from;
      if (from >= end) break;
      lead = indent + code_hang;
    }
    start = end + 1;
  }
  return out;
}

void examples(Block& out, std::span<const Snippet> xs, const Options& o, bool wrap) {
  struct Ex {
    Block code;
    Line comment;
  };
  std::vector<Ex> exs;
  for (const auto& x : xs) {
    Ex ex{code_lines(x.code, o, wrap), {}};
    if (!x.comment.empty()) ex.comment.add("// " + std::string(x.comment), Role::Note);
    if (x.std != Std::Unknown) {
      if (!ex.comment.empty()) ex.comment.add("  ");
      ex.comment.append(std_badge(to_string(x.std), x.std, o));
    }
    exs.push_back(std::move(ex));
  }
  // Comments sit in one column after the first code line, if they all fit.
  std::size_t first_w = 0;
  for (const auto& ex : exs)
    if (!ex.comment.empty()) first_w = std::max(first_w, ex.code.front().width());
  const std::size_t col = first_w + comment_gap;
  const bool aligned = std::ranges::all_of(exs, [&](const Ex& ex) {
    return ex.comment.empty() || col + ex.comment.width() <= o.width;
  });
  for (auto& ex : exs) {
    if (!ex.comment.empty()) {
      if (aligned) {
        ex.code.front().pad_to(col).append(ex.comment);
      } else {
        Line above = spaces(indent);
        above.append(ex.comment);
        out.push_back(std::move(above));
      }
    }
    push(out, std::move(ex.code));
  }
}

void gotchas(Block& out, std::span<const Gotcha> gs, const Options& o) {
  for (const auto& g : gs) {
    Line prefix = spaces(indent);
    switch (g.level) {
      case Level::Ub: prefix.add(sym(Sym::Ub, o.ascii), Role::Ub); break;
      case Level::Warn: prefix.add(sym(Sym::Warn, o.ascii), Role::Warn); break;
      case Level::Perf: prefix.add(sym(Sym::Perf, o.ascii), Role::Perf); break;
    }
    prefix.add(" ");
    push(out, flow(prefix, words(g.text), o.width, indent + 2));
  }
}

// "gcc 13 · clang 17 · msvc 19.29 · __cpp_lib_format" (PLAN §4.3): shown with
// --support, or with --std C++20 or later when the card has support data.
void support_line(Block& out, const Entry& e, const Options& o) {
  const Support& s = e.support;
  std::vector<Line> items;
  auto item = [&](std::string text) {
    if (!items.empty()) items.back().add(" " + std::string(sym(Sym::Dot, o.ascii)), Role::Badge);
    items.emplace_back().add(text, Role::Badge);
  };
  using P = std::pair<std::string_view, std::string_view>;
  for (auto [label, version] :
       {P{"gcc", s.gcc}, P{"clang", s.clang}, P{"apple clang", s.apple_clang}, P{"msvc", s.msvc}})
    if (!version.empty()) item(std::string(label) + " " + std::string(version));
  if (!s.ftm.empty()) item(std::string(s.ftm));

  if (items.empty()) {
    if (o.support) out.emplace_back().add("no support data yet", Role::Badge);
    return;
  }
  if (o.support || (o.std != Std::Unknown && o.std >= Std::Cpp20)) push(out, flow(Line{}, items, o.width, 0));
}

void also_line(Block& out, std::span<const std::string_view> also, const Options& o) {
  if (also.empty()) return;
  Line prefix;
  prefix.add("also:", Role::Note);
  std::vector<Line> items;
  for (auto name : also) items.emplace_back().add(name, Role::Note);
  Line lead = prefix;
  lead.add(" ");
  push(out, flow(lead, items, o.width, 6, 2));
}

std::string display_name(const IndexName& n) {
  const bool macro = n.kind == Kind::Macro ||
                     std::ranges::all_of(n.name, [](char c) {
                       return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
                     });
  return macro ? std::string(n.name) : "std::" + std::string(n.name);
}

}  // namespace

Std std_of_badge(std::string_view badge) {
  const auto at = badge.rfind("C++");
  if (at == std::string_view::npos) return Std::Unknown;
  return enum_from<Std>(std_names, badge.substr(at, 5)).value_or(Std::Unknown);
}

std::string reference_url(std::string_view path) {
  return std::string(cppreference_base) + std::string(path);
}

std::string header_url(std::string_view header) { return reference_url("header/" + std::string(header)); }

std::string card_url(const Catalog& cat, std::string_view name) {
  const std::int32_t i = card_for(cat, name);
  return i < 0 ? std::string{} : reference_url(cat.entries[static_cast<std::size_t>(i)].url);
}

Block card(const Catalog& cat, const Entry& e, std::span<const std::string_view> also,
           const Options& o) {
  Block out;
  const Parts p = parts_of(e, o);
  head(out, e, p, o);
  push(out, flow(Line{}, words(e.summary), o.width, 0));

  if (!p.signatures.empty()) {
    blank(out);
    signatures(out, p.signatures, o);
  }
  if (!p.members.empty()) {
    blank(out);
    members(out, cat, e, p.members, o);
  }
  if (!p.examples.empty()) {
    blank(out);
    examples(out, p.examples, o, true);
  }
  if (!e.gotchas.empty()) {
    blank(out);
    gotchas(out, e.gotchas, o);
  }
  hidden_line(out, p.hidden, o);

  blank(out);
  if (!e.complexity.empty()) out.emplace_back().add(e.complexity);
  support_line(out, e, o);
  Line hint;
  const std::string page = reference_url(e.url);
  hint.add("cxxman " + std::string(e.id), Role::Badge, page);
  if (e.kind == Kind::Class) hint.add("::<member>" + sep(o) + "--web", Role::Badge, page);
  else hint.add(" --web", Role::Badge, page);
  Block footer;
  if (!e.see_also.empty()) {
    std::vector<Line> items;
    for (auto name : e.see_also) {
      const std::string url = card_url(cat, name);
      items.emplace_back().add(name, url.empty() ? Role::Plain : Role::Link, url);
    }
    Line prefix;
    prefix.add("see: ");
    footer = flow(prefix, items, o.width, 5);
  }
  if (footer.empty() || !put_right(footer.back(), hint, o.width, 3)) {
    Line l;
    put_right(l, hint, o.width, 0);
    footer.push_back(std::move(l));
  }
  push(out, std::move(footer));
  also_line(out, also, o);
  return out;
}

Block examples_only(const Entry& e, const Options& o) {
  Block out;
  const Parts p = parts_of(e, o);
  head(out, e, p, o);
  if (!p.examples.empty()) {
    blank(out);
    examples(out, p.examples, o, false);
  }
  hidden_line(out, p.hidden, o);
  return out;
}

Block signatures_only(const Entry& e, const Options& o) {
  Block out;
  const Parts p = parts_of(e, o);
  head(out, e, p, o);
  if (!p.signatures.empty()) {
    blank(out);
    signatures(out, p.signatures, o);
  }
  hidden_line(out, p.hidden, o);
  return out;
}

Block brief(const Entry& e, const Options& o) {
  Line l;
  l.add(e.name, Role::Title, reference_url(e.url)).add(" ");
  l.add("<" + std::string(e.header) + ">", Role::Header, header_url(e.header));
  l.add(" " + std::string(sym(Sym::Dash, o.ascii)) + " ");
  const std::size_t room = o.width > l.width() ? o.width - l.width() : 1;
  l.add(truncate(e.summary, room, sym(Sym::Ellipsis, o.ascii)));
  return {l};
}

Block index_only(const Catalog& cat, std::span<const std::int32_t> rows,
                 std::span<const std::string_view> also, const Options& o) {
  Block out;
  if (rows.empty()) return out;
  const IndexName& first = cat.index[static_cast<std::size_t>(rows.front())];
  Line title;
  title.add(display_name(first), Role::Title).add("  ");
  if (rows.size() == 1) {
    title.add("<" + std::string(first.header) + ">", Role::Header, header_url(first.header));
    Line badge;
    if (first.since != Std::Unknown) badge = std_badge(to_string(first.since), first.since, o);
    title_block(out, std::move(title), badge, o.width);
    out.emplace_back().add("No card yet. Full reference:");
    out.emplace_back().add(reference_url(first.url));
  } else {
    title.add("in " + std::to_string(rows.size()) + " places", Role::Badge);
    out.push_back(std::move(title));
    out.emplace_back().add("No card yet. Full reference:");
    const std::size_t shown = std::min(rows.size(), max_index_rows);
    std::size_t header_w = 0, url_w = 0;
    for (std::size_t i = 0; i < shown; ++i) {
      const IndexName& n = cat.index[static_cast<std::size_t>(rows[i])];
      header_w = std::max(header_w, n.header.size() + 2);
      url_w = std::max(url_w, display_width(reference_url(n.url)));
    }
    const std::size_t col = indent + header_w + 2;
    const bool columns = col + url_w <= o.width;
    for (std::size_t i = 0; i < shown; ++i) {
      const IndexName& n = cat.index[static_cast<std::size_t>(rows[i])];
      Line l = spaces(indent);
      l.add("<" + std::string(n.header) + ">", Role::Header, header_url(n.header));
      if (columns) {
        l.pad_to(col).add(reference_url(n.url));
        out.push_back(std::move(l));
      } else {
        out.push_back(std::move(l));
        out.push_back(spaces(indent + 2).add(reference_url(n.url)));
      }
    }
    if (rows.size() > shown)
      out.push_back(spaces(indent).add(std::string(sym(Sym::Ellipsis, o.ascii)) + " " +
                                           std::to_string(rows.size() - shown) + " more",
                                       Role::Badge));
  }
  also_line(out, also, o);
  return out;
}

Block not_found(std::string_view input, std::span<const std::string_view> suggestions,
                const Options& o) {
  Block out;
  out.emplace_back().add("cxxman: no entry for '" + std::string(input) + "'");
  if (!suggestions.empty()) {
    std::vector<Line> items;
    for (auto s : suggestions) items.emplace_back().add(s);
    Line prefix;
    prefix.add("  did you mean  ");
    push(out, flow(prefix, items, o.width, 16, 2));
  }
  return out;
}

Block ambiguous(std::string_view input, std::span<const std::string_view> candidates,
                const Options& o) {
  Block out;
  push(out, flow(Line{}, words("cxxman: '" + std::string(input) +
                               "' is a member of several classes; pick one:"),
                 o.width, 2));
  std::vector<Line> items;
  for (auto c : candidates) items.emplace_back().add(c);
  push(out, flow(spaces(indent), items, o.width, indent, 2));
  return out;
}

}  // namespace cxxman::render
