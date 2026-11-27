#include "render/card.hpp"

#include "render/highlight.hpp"
#include "render/style.hpp"

#include <algorithm>
#include <string>
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

Line spaces(std::size_t n) {
  Line l;
  l.add(std::string(n, ' '));
  return l;
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

void signatures(Block& out, const Entry& e, const Options& o) {
  std::size_t text_w = 0;
  std::vector<Line> notes;
  for (const auto& s : e.signatures) {
    text_w = std::max(text_w, display_width(s.text));
    Line n;
    if (s.since != Std::Unknown) n.add(to_string(s.since), Role::Badge);
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
  for (std::size_t i = 0; i < e.signatures.size(); ++i) {
    Line l = spaces(indent);
    l.add(e.signatures[i].text, Role::Sig);
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
      const auto& s = e.signatures[i];
      if (s.since != Std::Unknown) {
        Line badge;
        badge.add(to_string(s.since), Role::Badge);
        if (!s.note.empty()) badge.add(" " + std::string(sym(Sym::Dot, o.ascii)), Role::Badge);
        items.push_back(std::move(badge));
      }
      for (auto& w : words(s.note, Role::Note)) items.push_back(std::move(w));
      push(out, flow(spaces(note_indent), items, o.width, note_indent));
    }
  }
}

void members(Block& out, const Catalog& cat, const Entry& e, const Options& o) {
  std::size_t label_w = 0;
  for (const auto& g : e.members) label_w = std::max(label_w, display_width(g.title));
  const std::size_t col = indent + label_w + 2;
  for (const auto& g : e.members) {
    Line prefix = spaces(indent);
    prefix.add(g.title).pad_to(col);
    std::vector<Line> items;
    for (std::string_view name : g.names) {
      // A member with its own card is a link: "vector::erase".
      const bool card = card_for(cat, std::string(e.id) + "::" + std::string(name)) >= 0 ||
                        (name.starts_with("std::") && card_for(cat, name) >= 0);
      items.emplace_back().add(name, card ? Role::Link : Role::Plain);
    }
    Block lines = flow(prefix, items, o.width, col, 2);
    if (g.since != Std::Unknown) {
      Line badge;
      badge.add(to_string(g.since), Role::Badge);
      if (!put_right(lines.back(), badge, o.width)) {
        Line extra;
        put_right(extra, badge, o.width, 0);
        lines.push_back(std::move(extra));
      }
    }
    push(out, std::move(lines));
  }
}

// Code lines of one example, highlighted; lines wider than the card wrap at
// a space with a deeper indent.
Block code_lines(std::string_view code, const Options& o) {
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
      if (lead + display_width(rest) > o.width) {
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

void examples(Block& out, const Entry& e, const Options& o) {
  struct Ex {
    Block code;
    Line comment;
  };
  std::vector<Ex> exs;
  for (const auto& x : e.examples) {
    Ex ex{code_lines(x.code, o), {}};
    if (!x.comment.empty()) ex.comment.add("// " + std::string(x.comment), Role::Note);
    if (x.std != Std::Unknown) {
      if (!ex.comment.empty()) ex.comment.add("  ");
      ex.comment.add(to_string(x.std), Role::Badge);
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

std::string url_of(std::string_view path) { return std::string(cppreference_base) + std::string(path); }

}  // namespace

Block card(const Catalog& cat, const Entry& e, std::span<const std::string_view> also,
           const Options& o) {
  Block out;
  Line title;
  title.add(title_text(e), Role::Title).add("  ").add("<" + std::string(e.header) + ">", Role::Header);
  Line badges;
  if (e.since != Std::Unknown) badges.add(to_string(e.since), Role::Badge);
  for (auto b : e.badges) {
    if (!badges.empty()) badges.add(sep(o), Role::Badge);
    badges.add(b, Role::Badge);
  }
  title_block(out, std::move(title), badges, o.width);
  push(out, flow(Line{}, words(e.summary), o.width, 0));

  if (!e.signatures.empty()) {
    blank(out);
    signatures(out, e, o);
  }
  if (!e.members.empty()) {
    blank(out);
    members(out, cat, e, o);
  }
  if (!e.examples.empty()) {
    blank(out);
    examples(out, e, o);
  }
  if (!e.gotchas.empty()) {
    blank(out);
    gotchas(out, e.gotchas, o);
  }

  blank(out);
  if (!e.complexity.empty()) out.emplace_back().add(e.complexity);
  Line hint;
  hint.add("cxxman " + std::string(e.id), Role::Badge);
  if (e.kind == Kind::Class) hint.add("::<member>" + sep(o) + "--web", Role::Badge);
  else hint.add(" --web", Role::Badge);
  Block footer;
  if (!e.see_also.empty()) {
    std::vector<Line> items;
    for (auto name : e.see_also)
      items.emplace_back().add(name, card_for(cat, name) >= 0 ? Role::Link : Role::Plain);
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

Block index_only(const Catalog& cat, std::span<const std::int32_t> rows,
                 std::span<const std::string_view> also, const Options& o) {
  Block out;
  if (rows.empty()) return out;
  const IndexName& first = cat.index[static_cast<std::size_t>(rows.front())];
  Line title;
  title.add(display_name(first), Role::Title).add("  ");
  if (rows.size() == 1) {
    title.add("<" + std::string(first.header) + ">", Role::Header);
    Line badge;
    if (first.since != Std::Unknown) badge.add(to_string(first.since), Role::Badge);
    title_block(out, std::move(title), badge, o.width);
    out.emplace_back().add("No card yet. Full reference:");
    out.emplace_back().add(url_of(first.url));
  } else {
    title.add("in " + std::to_string(rows.size()) + " places", Role::Badge);
    out.push_back(std::move(title));
    out.emplace_back().add("No card yet. Full reference:");
    const std::size_t shown = std::min(rows.size(), max_index_rows);
    std::size_t header_w = 0, url_w = 0;
    for (std::size_t i = 0; i < shown; ++i) {
      const IndexName& n = cat.index[static_cast<std::size_t>(rows[i])];
      header_w = std::max(header_w, n.header.size() + 2);
      url_w = std::max(url_w, display_width(url_of(n.url)));
    }
    const std::size_t col = indent + header_w + 2;
    const bool columns = col + url_w <= o.width;
    for (std::size_t i = 0; i < shown; ++i) {
      const IndexName& n = cat.index[static_cast<std::size_t>(rows[i])];
      Line l = spaces(indent);
      l.add("<" + std::string(n.header) + ">", Role::Header);
      if (columns) {
        l.pad_to(col).add(url_of(n.url));
        out.push_back(std::move(l));
      } else {
        out.push_back(std::move(l));
        out.push_back(spaces(indent + 2).add(url_of(n.url)));
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
