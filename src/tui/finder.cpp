#include "tui/finder.hpp"

#include "render/lists.hpp"
#include "term/screen.hpp"

#include <algorithm>

namespace cxxman::tui {

namespace {

using C = term::Key::Code;
using K = Item::Kind;
using render::Line;
using render::Role;

constexpr std::size_t side_min = 100, preview_min = 60;

// `l` cut to `w` columns (no ellipsis: the cut is the cell edge).
Line clip(const Line& l, std::size_t w) {
  if (l.width() <= w) return l;
  Line out;
  for (const auto& sp : l.spans()) {
    const std::size_t left = w - out.width();
    if (left == 0) break;
    if (render::display_width(sp.text) <= left) {
      out.add(sp.text, sp.role, sp.url);
    } else {
      out.add(render::truncate(sp.text, left, ""), sp.role, sp.url);
      break;
    }
  }
  return out;
}

// Box drawing, with ASCII stand-ins.
struct Box {
  std::string_view h, v, tl, tr, bl, br, tee_down, tee_up;
};
constexpr Box unicode_box{"─", "│", "╭", "╮", "╰", "╯", "┬", "┴"};
constexpr Box ascii_box{"-", "|", "+", "+", "+", "+", "+", "+"};

std::string repeat(std::string_view s, std::size_t n) {
  std::string out;
  for (std::size_t i = 0; i < n; ++i) out += s;
  return out;
}

// --std cycles none, 11, 14, ..., 26, none (Ctrl-S).
Std next_std(Std s) {
  switch (s) {
    case Std::Unknown: return Std::Cpp11;
    case Std::Cpp11: return Std::Cpp14;
    case Std::Cpp14: return Std::Cpp17;
    case Std::Cpp17: return Std::Cpp20;
    case Std::Cpp20: return Std::Cpp23;
    case Std::Cpp23: return Std::Cpp26;
    default: return Std::Unknown;
  }
}

}  // namespace

Finder::Finder(const Catalog& cat, const FinderModel& model, FinderSettings s)
    : cat_(&cat), model_(&model), settings_(s), known_(known_names(cat)) {
  for (const auto& n : known_) ++per_header_[n.header];
  header_count_ = model.find("<").size();
  requery();
}

void Finder::requery() {
  matches_ = model_->find(query_);
  sel_ = top_ = 0;
}

const Item* Finder::selected() const {
  return matches_.empty() ? nullptr : &model_->item(matches_[sel_].item);
}

Finder::Layout Finder::layout(term::Size s) const {
  Layout l;
  const std::size_t h = s.rows, w = s.cols;
  const bool preview = preview_on_.value_or(w >= preview_min);
  if (!preview || h < 8) {
    l.mode = Mode::List;
    l.list_rows = h > 2 ? h - 2 : 1;
    l.list_w = w;
  } else if (w >= side_min) {
    l.mode = Mode::Side;
    // The preview keeps the 60 columns the cards are made for (+ margins).
    l.list_w = std::clamp<std::size_t>(w - 65, 30, 44);
    l.preview_w = w - l.list_w - 3;
    l.list_rows = l.preview_rows = h - 4;
  } else {
    l.mode = Mode::Stacked;
    const std::size_t room = h - 3;  // the query, the rule and the help line
    l.list_rows = std::max<std::size_t>(3, room * 2 / 5);
    l.preview_rows = room - l.list_rows;
    l.list_w = l.preview_w = w;
  }
  return l;
}

void Finder::move(std::ptrdiff_t by, std::size_t rows) {
  if (matches_.empty()) return;
  const auto last = static_cast<std::ptrdiff_t>(matches_.size()) - 1;
  sel_ = static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(static_cast<std::ptrdiff_t>(sel_) + by, 0, last));
  if (sel_ < top_) top_ = sel_;
  if (rows > 0 && sel_ >= top_ + rows) top_ = sel_ - rows + 1;
}

Finder::Action Finder::handle(const term::Key& k, term::Size s) {
  const Layout l = layout(s);
  const auto page = static_cast<std::ptrdiff_t>(std::max<std::size_t>(l.list_rows, 1));
  switch (k.code) {
    case C::Char:
      term::append_utf8(query_, k.ch);
      requery();
      return Action::None;
    case C::Backspace:
      if (query_.empty()) return Action::None;
      while (!query_.empty() && (static_cast<unsigned char>(query_.back()) & 0xC0u) == 0x80u) query_.pop_back();
      if (!query_.empty()) query_.pop_back();
      requery();
      return Action::None;
    case C::Up: move(-1, l.list_rows); return Action::None;
    case C::Down: move(1, l.list_rows); return Action::None;
    case C::PageUp: move(-page, l.list_rows); return Action::None;
    case C::PageDown: move(page, l.list_rows); return Action::None;
    case C::Home: move(-static_cast<std::ptrdiff_t>(sel_), l.list_rows); return Action::None;
    case C::End: move(static_cast<std::ptrdiff_t>(matches_.size()), l.list_rows); return Action::None;
    case C::Tab: preview_on_ = l.mode == Mode::List; return Action::None;
    case C::Enter: return selected() ? Action::Open : Action::None;
    case C::Esc: return Action::Quit;
    case C::Resize: move(0, l.list_rows); return Action::None;
    case C::Ctrl: break;
    default: return Action::None;
  }
  switch (k.ch) {
    case 'c': return Action::Quit;
    case 'p': move(-1, l.list_rows); break;
    case 'n': move(1, l.list_rows); break;
    case 'u':
      query_.clear();
      requery();
      break;
    case 'e': examples_ = !examples_; break;
    case 's': settings_.std = next_std(settings_.std); break;
    case 'o': return selected() ? Action::Print : Action::None;
    case 'y': return selected() ? Action::Copy : Action::None;
    case 'w': return selected() ? Action::Web : Action::None;
    default: break;
  }
  return Action::None;
}

std::string Finder::paint(const Line& line) const {
  std::string s = render::paint(render::Block{line}, settings_.style);
  if (!s.empty() && s.back() == '\n') s.pop_back();
  return s;
}

// " cxxman ❯ query" and, on the right, "C++17 · 6 / 2,611".
Line Finder::prompt(std::size_t cols) const {
  Line l;
  l.add(" ").add("cxxman", Role::Accent).add(settings_.ascii ? " > " : " ❯ ", Role::Accent).add(query_);
  const bool headers = !query_.empty() && query_.find_first_not_of(' ') != std::string::npos &&
                       query_[query_.find_first_not_of(' ')] == '<';
  Line right;
  if (settings_.std != Std::Unknown)
    right.add(to_string(settings_.std), Role::Newer).add(" " + std::string(render::sym(render::Sym::Dot, settings_.ascii)) + " ", Role::Note);
  right.add(render::group_digits(matches_.size()) + " / " +
                render::group_digits(headers ? header_count_ : model_->size()),
            Role::Note);
  right.add(" ");
  render::put_right(l, right, cols, 2);
  return l;
}

std::size_t Finder::cursor_column() const {
  return render::display_width(" cxxman ❯ ") + render::display_width(query_) + 1;
}

// The keys, as many whole items as fit.
Line Finder::help(std::size_t cols) const {
  const std::string_view items[] = {settings_.ascii ? "up/down move" : "↑↓ move",
                                    settings_.ascii ? "enter open" : "⏎ open",
                                    "tab preview", "^E examples", "^Y copy", "^W web", "^S std", "esc quit"};
  std::string text;
  for (auto item : items) {
    if (render::display_width(text) + 2 + render::display_width(item) > cols) break;
    (text += text.empty() ? " " : "  ") += item;
  }
  return Line().add(text, Role::Note);
}

std::vector<Line> Finder::list(std::size_t rows, std::size_t width) const {
  std::vector<Line> out;
  if (matches_.empty()) {
    out.push_back(Line().add(" no matches", Role::Note));
    return out;
  }
  const std::size_t end = std::min(matches_.size(), top_ + rows);
  // What each row shows after the name: <header>, recipe, compare, or a count.
  auto detail = [&](const Item& it) -> std::pair<std::string, Role> {
    switch (it.kind) {
      case K::Card: case K::Name: return {"<" + std::string(it.header) + ">", Role::Header};
      case K::Recipe: return {"recipe", Role::Badge};
      case K::Compare: return {"compare", Role::Badge};
      case K::Header: {
        const auto it2 = per_header_.find(it.name);
        const std::size_t n = it2 == per_header_.end() ? 0 : it2->second;
        return {render::group_digits(n) + (n == 1 ? " name" : " names"), Role::Note};
      }
    }
    return {};
  };
  auto shown = [](const Item& it) {
    return it.kind == K::Header ? "<" + std::string(it.name) + ">" : std::string(it.name);
  };
  std::size_t name_w = 0, detail_w = 0;
  for (std::size_t i = top_; i < end; ++i) {
    const Item& it = model_->item(matches_[i].item);
    name_w = std::max(name_w, render::display_width(shown(it)));
    detail_w = std::max(detail_w, render::display_width(detail(it).first));
  }
  // Marker, name, two spaces, detail, one space; the name gives way first.
  const std::size_t room = width > detail_w + 4 ? width - detail_w - 4 : 0;
  name_w = std::min(name_w, std::max<std::size_t>(room, 8));
  const auto ellipsis = render::sym(render::Sym::Ellipsis, settings_.ascii);
  for (std::size_t i = top_; i < end; ++i) {
    const Item& it = model_->item(matches_[i].item);
    const bool sel = i == sel_;
    Line l;
    if (sel)
      l.add(settings_.ascii ? ">" : "▌", Role::Accent);
    else if (it.kind == K::Recipe)
      l.add("#", Role::Badge);
    else if (it.kind == K::Compare)
      l.add("=", Role::Badge);
    else
      l.add(" ");
    l.add(render::truncate(shown(it), name_w, ellipsis),
          it.kind == K::Header ? Role::Header : sel ? Role::Title : Role::Plain);
    l.pad_to(1 + name_w + 2);
    const auto [text, role] = detail(it);
    l.add(text, role);
    out.push_back(clip(l, width));
  }
  return out;
}

const std::vector<Line>& Finder::preview(std::size_t width) {
  const Item* it = selected();
  if (!it) {
    preview_lines_.clear();
    cached_.valid = false;
    return preview_lines_;
  }
  const std::uint32_t item = matches_[sel_].item;
  if (cached_.valid && cached_.item == item && cached_.width == width && cached_.std == settings_.std &&
      cached_.examples == examples_)
    return preview_lines_;
  cached_ = {item, width, settings_.std, examples_, true};

  // The renderers are made for 60 to 100 columns; a narrower cell is cut.
  render::Options o{std::clamp<std::size_t>(width, 60, 100), settings_.ascii, settings_.std, settings_.strict};
  render::Block b;
  const auto i = static_cast<std::size_t>(it->index);
  switch (it->kind) {
    case K::Card: {
      const Entry& e = cat_->entries[i];
      b = examples_ && !e.examples.empty() ? render::examples_only(e, o) : render::card(*cat_, e, {}, o);
      break;
    }
    case K::Name: {
      std::vector<std::int32_t> rows;
      for (std::size_t r = 0; r < cat_->index.size(); ++r)
        if (cat_->index[r].name == it->name) rows.push_back(static_cast<std::int32_t>(r));
      b = render::index_only(*cat_, rows, {}, o);
      break;
    }
    case K::Header: {
      std::vector<NameInfo> names;
      for (const auto& n : known_)
        if (n.header == it->name) names.push_back(n);
      auto h = std::ranges::lower_bound(cat_->headers, it->name, {}, &Header::header);
      const Header* hp = h != cat_->headers.end() && h->header == it->name ? &*h : nullptr;
      b = render::header_index(*cat_, it->name, hp, names, o);
      break;
    }
    case K::Recipe: {
      const Recipe& r = cat_->recipes[i];
      b.push_back(Line().add(std::string(r.title), Role::Title));
      for (const auto& line : render::wrap(r.summary, o.width)) b.push_back(Line().add(line));
      break;
    }
    case K::Compare: {
      const Compare& c = cat_->compares[i];
      b.push_back(Line().add(std::string(c.title), Role::Title));
      break;
    }
  }
  preview_lines_.clear();
  for (const auto& l : b) preview_lines_.push_back(clip(l, width));
  return preview_lines_;
}

std::vector<std::string> Finder::frame(term::Size s) {
  const Layout l = layout(s);
  move(0, l.list_rows);  // keep the selection on screen after a resize
  std::vector<std::string> rows;
  rows.push_back(paint(clip(prompt(s.cols), s.cols)));
  const auto items = list(l.list_rows, l.mode == Mode::Side ? l.list_w : s.cols);
  const Box& box = settings_.ascii ? ascii_box : unicode_box;

  if (l.mode == Mode::Side) {
    const auto& pv = preview(l.preview_w - 2);
    rows.push_back(paint(Line().add(std::string(box.tl) + repeat(box.h, l.list_w) + std::string(box.tee_down) +
                                        repeat(box.h, l.preview_w) + std::string(box.tr),
                                    Role::Note)));
    for (std::size_t r = 0; r < l.list_rows; ++r) {
      Line row;
      row.add(box.v, Role::Note);
      if (r < items.size()) row.append(items[r]);
      row.pad_to(1 + l.list_w);
      row.add(box.v, Role::Note).add(" ");
      if (r < pv.size()) row.append(pv[r]);
      row.pad_to(s.cols - 1);
      row.add(box.v, Role::Note);
      rows.push_back(paint(row));
    }
    rows.push_back(paint(Line().add(std::string(box.bl) + repeat(box.h, l.list_w) + std::string(box.tee_up) +
                                        repeat(box.h, l.preview_w) + std::string(box.br),
                                    Role::Note)));
  } else {
    for (std::size_t r = 0; r < l.list_rows; ++r) rows.push_back(r < items.size() ? paint(items[r]) : "");
    if (l.mode == Mode::Stacked) {
      const auto& pv = preview(s.cols);
      rows.push_back(paint(Line().add(repeat(box.h, s.cols), Role::Note)));
      for (std::size_t r = 0; r < l.preview_rows; ++r) rows.push_back(r < pv.size() ? paint(pv[r]) : "");
    }
  }
  if (s.rows >= 3) rows.push_back(paint(help(s.cols)));
  rows.resize(std::min(rows.size(), s.rows));
  return rows;
}

Finder::Action run(term::Tty& tty, Finder& f) {
  term::Screen screen;
  for (;;) {
    const term::Size s = tty.size();
    tty.write("\x1b[?25l" + screen.update(f.frame(s)) + term::move_to(1, std::min(f.cursor_column(), s.cols)) +
              "\x1b[?25h");
    const term::Key k = tty.read();
    if (k.code == C::Eof) return Finder::Action::Quit;
    if (k.code == C::Resize) screen.invalidate();
    const auto a = f.handle(k, tty.size());
    if (a != Finder::Action::None) return a;
  }
}

}  // namespace cxxman::tui
