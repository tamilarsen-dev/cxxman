#include "render/lists.hpp"

#include "render/style.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace cxxman::render {

namespace {

constexpr std::size_t indent = 2;
// Name columns are as wide as the widest name up to a cap; longer names push
// the rest of their own line right instead of widening every line.
constexpr std::size_t max_name_col = 24;  // search
constexpr std::size_t max_list_col = 32;  // --list

// The column width after one more name of width `name_w`.
std::size_t fit(std::size_t w, std::size_t name_w, std::size_t cap) {
  return name_w <= cap ? std::max(w, name_w) : w;
}

void blank(Block& out) {
  if (!out.empty() && !out.back().empty()) out.emplace_back();
}

void push(Block& out, Block lines) {
  for (auto& l : lines) out.push_back(std::move(l));
}

// Spaces up to `col`, or two if the line already reaches it.
void column(Line& l, std::size_t col) {
  if (l.width() + 2 > col)
    l.add("  ");
  else
    l.pad_to(col);
}

std::string bracket(std::string_view header) { return "<" + std::string(header) + ">"; }

std::string plural(std::size_t n, std::string_view one, std::string_view many) {
  return group_digits(n) + " " + std::string(n == 1 ? one : many);
}

// "38 of 105 names have cards".
std::string covered_text(std::size_t covered, std::size_t total) {
  return group_digits(covered) + " of " + plural(total, "name", "names") +
         (total == 1 ? " has a card" : " have cards");
}

// A labelled group: `label` padded to `col`, then the items, wrapped under
// the column; a `since` badge right-aligned on the last line.
void group(Block& out, std::string_view label, Role label_role, const std::vector<Line>& items,
           Std since, std::size_t col, const Options& o) {
  Line prefix = spaces(indent);
  prefix.add(label, label_role);
  column(prefix, col);
  Block lines = flow(prefix, items, o.width, col, 2);
  if (since != Std::Unknown) {
    Line badge;
    badge.add(to_string(since), Role::Badge);
    if (!put_right(lines.back(), badge, o.width)) {
      Line extra;
      put_right(extra, badge, o.width, 0);
      lines.push_back(std::move(extra));
    }
  }
  push(out, std::move(lines));
}

Line name_item(std::string_view name, bool card) {
  Line l;
  l.add(name, card ? Role::Link : Role::Plain);
  return l;
}

// Distinct names, and how many of them have a card.
std::pair<std::size_t, std::size_t> count_names(std::span<const NameInfo> names) {
  std::size_t total = 0, covered = 0;
  for (std::size_t i = 0; i < names.size();) {
    std::size_t j = i;
    bool card = false;
    for (; j < names.size() && names[j].name == names[i].name; ++j) card = card || names[j].entry >= 0;
    ++total;
    if (card) ++covered;
    i = j;
  }
  return {total, covered};
}

}  // namespace

Block search_results(const Catalog& cat, std::span<const search::Hit> hits, const Options& o) {
  struct Row {
    std::string_view marker{}, name{};
    bool openable = false;
    std::string where{};
    Role where_role = Role::Badge;
    std::string summary{};
    Role summary_role = Role::Plain;
  };
  std::vector<Row> rows;
  for (const auto& h : hits) {
    const auto i = static_cast<std::size_t>(h.index);
    Row r{" ", search::hit_name(cat, h)};
    switch (h.target) {
      case Target::Entry:
        r.openable = true;
        r.where = bracket(cat.entries[i].header);
        r.where_role = Role::Header;
        r.summary = cat.entries[i].summary;
        break;
      case Target::Recipe:
        r.marker = "#";
        r.openable = true;
        r.where = "recipe";
        r.summary = cat.recipes[i].summary;
        break;
      case Target::Compare:
        r.marker = o.ascii ? "=" : "≠";
        r.openable = true;
        r.where = "compare";
        r.summary = cat.compares[i].title;
        break;
      case Target::Index:
        r.where = bracket(cat.index[i].header);
        r.where_role = Role::Header;
        r.summary = "no card yet";
        if (h.headers > 1)
          r.summary += " " + std::string(sym(Sym::Dot, o.ascii)) + " in " +
                       std::to_string(h.headers) + " headers";
        r.summary_role = Role::Badge;
        break;
    }
    rows.push_back(std::move(r));
  }
  std::size_t name_w = 0, where_w = 0;
  for (const auto& r : rows) {
    name_w = fit(name_w, display_width(r.name), max_name_col);
    where_w = std::max(where_w, display_width(r.where));
  }
  const std::size_t where_col = indent + name_w + 2, summary_col = where_col + where_w + 2;
  Block out;
  for (const auto& r : rows) {
    Line l;
    l.add(r.marker, Role::Badge).add(" ");
    l.add(r.name, r.openable ? Role::Link : Role::Plain);
    column(l, where_col);
    l.add(r.where, r.where_role);
    column(l, summary_col);
    if (l.width() < o.width)
      l.add(truncate(r.summary, o.width - l.width(), sym(Sym::Ellipsis, o.ascii)), r.summary_role);
    out.push_back(std::move(l));
  }
  return out;
}

Block header_index(const Catalog& cat, std::string_view header, const Header* h,
                   std::span<const NameInfo> names, const Options& o) {
  Block out;
  Line title;
  title.add(bracket(header), Role::Header);
  if (h && !h->summary.empty()) {
    title.add("  ");
    push(out, flow(title, words(h->summary), o.width, 0));
  } else {
    out.push_back(std::move(title));
  }

  // Grouped names first; every other name of the header under "Other".
  std::vector<std::string_view> grouped;
  if (h)
    for (const auto& g : h->groups) grouped.insert(grouped.end(), g.names.begin(), g.names.end());
  std::ranges::sort(grouped);
  std::vector<Line> rest;
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (i > 0 && names[i].name == names[i - 1].name) continue;
    if (!std::ranges::binary_search(grouped, names[i].name))
      rest.push_back(name_item(names[i].name, names[i].entry >= 0));
  }
  const std::string_view rest_label = h && !h->groups.empty() ? "Other" : "Names";
  std::size_t label_w = rest.empty() ? 0 : display_width(rest_label);
  if (h)
    for (const auto& g : h->groups) label_w = std::max(label_w, display_width(g.title));
  const std::size_t col = indent + label_w + 2;

  blank(out);
  if (h)
    for (const auto& g : h->groups) {
      std::vector<Line> items;
      for (auto n : g.names) items.push_back(name_item(n, card_for(cat, n) >= 0));
      group(out, g.title, Role::Plain, items, g.since, col, o);
    }
  if (!rest.empty()) group(out, rest_label, Role::Plain, rest, Std::Unknown, col, o);

  blank(out);
  const auto [total, covered] = count_names(names);
  Line footer;
  footer.add(covered_text(covered, total));
  Line hint;
  hint.add("cxxman -H " + std::string(header) + " --web", Role::Badge);
  if (!put_right(footer, hint, o.width, 3)) {
    out.push_back(std::move(footer));
    footer = Line{};
    put_right(footer, hint, o.width, 0);
  }
  out.push_back(std::move(footer));
  return out;
}

Block name_list(std::span<const NameInfo> names, [[maybe_unused]] const Options& o) {
  std::size_t name_w = 0, header_w = 0, kind_w = 0;
  for (const auto& n : names) {
    name_w = fit(name_w, display_width(n.name), max_list_col);
    header_w = std::max(header_w, n.header.size() + 2);
    kind_w = std::max(kind_w, to_string(n.kind).size());
  }
  const std::size_t header_col = name_w + 2, kind_col = header_col + header_w + 2,
                    since_col = kind_col + kind_w + 2;
  Block out;
  for (const auto& n : names) {
    Line l;
    l.add(n.name, n.entry >= 0 ? Role::Link : Role::Plain);
    column(l, header_col);
    l.add(bracket(n.header), Role::Header);
    if (n.kind != Kind::Unknown) {
      column(l, kind_col);
      l.add(to_string(n.kind));
    }
    if (n.since != Std::Unknown) {
      column(l, since_col);
      l.add(to_string(n.since), Role::Badge);
    }
    out.push_back(std::move(l));
  }
  blank(out);
  out.emplace_back().add(plural(names.size(), "name", "names"), Role::Badge);
  return out;
}

Block whats_new(std::span<const NameInfo> names, Std s, const Options& o) {
  std::map<std::string_view, std::vector<const NameInfo*>> by_header;
  for (const auto& n : names) by_header[n.header].push_back(&n);
  const auto [total, covered] = count_names(names);

  Block out;
  std::string head = std::string(to_string(s)) + ": " + plural(total, "name", "names") +
                     " across " + plural(by_header.size(), "header", "headers") + " (" +
                     group_digits(covered) + (covered == 1 ? " with a card)" : " with cards)");
  push(out, flow(Line{}, words(head), o.width, 0));
  blank(out);

  std::size_t label_w = 0;
  for (const auto& [h, _] : by_header) label_w = std::max(label_w, h.size() + 2);
  const std::size_t col = indent + label_w + 2;
  for (const auto& [h, list] : by_header) {
    std::vector<Line> items;
    for (const auto* n : list) items.push_back(name_item(n->name, n->entry >= 0));
    group(out, bracket(h), Role::Header, items, Std::Unknown, col, o);
  }
  return out;
}

Block coverage(std::span<const NameInfo> names, const Options& o) {
  struct Row {
    std::string_view header{};
    std::size_t covered = 0, total = 0;
  };
  std::map<std::string_view, Row> by_header;
  for (const auto& n : names) {
    Row& r = by_header[n.header];
    r.header = n.header;
    ++r.total;
    if (n.entry >= 0) ++r.covered;
  }
  std::vector<Row> rows;
  for (const auto& [_, r] : by_header) rows.push_back(r);
  std::ranges::sort(rows, [](const Row& a, const Row& b) {
    return std::tuple(b.total - b.covered, a.header) < std::tuple(a.total - a.covered, b.header);
  });

  const auto [total, covered] = count_names(names);
  const std::size_t permille = total == 0 ? 0 : covered * 1000 / total;
  Block out;
  out.emplace_back().add(covered_text(covered, total) + " (" + std::to_string(permille / 10) + "." +
                         std::to_string(permille % 10) + "%)");
  blank(out);

  std::size_t label_w = 0, covered_w = 0, total_w = 0;
  for (const auto& r : rows) {
    label_w = std::max(label_w, r.header.size() + 2);
    covered_w = std::max(covered_w, group_digits(r.covered).size());
    total_w = std::max(total_w, group_digits(r.total).size());
  }
  for (const auto& r : rows) {
    Line l = spaces(indent);
    l.add(bracket(r.header), Role::Header);
    const std::string c = group_digits(r.covered), t = group_digits(r.total);
    l.pad_to(indent + label_w + 2 + covered_w - c.size());
    l.add(c).add(" / ").add(std::string(total_w - t.size(), ' ') + t);
    out.push_back(std::move(l));
  }
  blank(out);
  Line hint;
  put_right(hint, Line{}.add("cxxman --coverage --missing <header>", Role::Badge), o.width, 0);
  out.push_back(std::move(hint));
  return out;
}

Block missing(std::string_view header, std::span<const NameInfo> names, const Options& o) {
  std::vector<Line> items;
  for (const auto& n : names)
    if (n.entry < 0) items.push_back(name_item(n.name, false));
  Block out;
  if (items.empty()) {
    out.emplace_back().add(bracket(header), Role::Header).add(": every name has a card");
    return out;
  }
  Line head;
  head.add(bracket(header), Role::Header)
      .add(": " + group_digits(items.size()) + " of " + plural(names.size(), "name", "names") +
           (items.size() == 1 ? " has" : " have") + " no card");
  out.push_back(std::move(head));
  blank(out);
  push(out, flow(spaces(indent), items, o.width, indent, 2));
  return out;
}

}  // namespace cxxman::render
