#include "tui/pager.hpp"

#include "render/layout.hpp"
#include "term/screen.hpp"

#include <algorithm>
#include <cstddef>

namespace cxxman::tui {

namespace {

using C = term::Key::Code;
using term::move_to;

// `s` without CSI (ESC [ ... final) and OSC (ESC ] ... ST or BEL) sequences.
std::string strip_escapes(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (std::size_t i = 0; i < s.size();) {
    if (s[i] != '\x1b') {
      out += s[i++];
      continue;
    }
    if (i + 1 >= s.size()) break;
    const char kind = s[i + 1];
    i += 2;
    if (kind == '[') {
      while (i < s.size() && !(s[i] >= 0x40 && s[i] <= 0x7E)) ++i;
      ++i;
    } else if (kind == ']') {
      while (i < s.size() && s[i] != '\x07' && !(s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '\\')) ++i;
      i += i < s.size() && s[i] == '\x1b' ? 2 : 1;
    }
  }
  return out;
}

std::string fold(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

void pop_utf8(std::string& s) {
  while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0u) == 0x80u) s.pop_back();
  if (!s.empty()) s.pop_back();
}

constexpr std::size_t npos = static_cast<std::size_t>(-1);

}  // namespace

Pager::Pager(std::string_view text) {
  while (!text.empty()) {
    const auto nl = text.find('\n');
    const auto line = text.substr(0, nl);
    lines_.push_back(line.ends_with('\r') ? line.substr(0, line.size() - 1) : line);
    plain_.push_back(strip_escapes(lines_.back()));
    folded_.push_back(fold(plain_.back()));
    if (nl == std::string_view::npos) break;
    text.remove_prefix(nl + 1);
  }
}

std::size_t Pager::max_top(std::size_t rows) const {
  const auto b = body(rows);
  return lines_.size() > b ? lines_.size() - b : 0;
}

void Pager::scroll(std::ptrdiff_t by, std::size_t rows) {
  const auto top = static_cast<std::ptrdiff_t>(top_) + by;
  top_ = std::min(static_cast<std::size_t>(std::max<std::ptrdiff_t>(top, 0)), max_top(rows));
}

// Moves to the first line from `from` on (or back) that holds the query.
bool Pager::search(std::size_t from, bool forward, std::size_t rows) {
  for (std::size_t i = from; i < lines_.size(); forward ? ++i : --i) {
    if (folded_[i].find(query_) != std::string::npos) {
      match_ = i;
      top_ = std::min(i, max_top(rows));
      return true;
    }
    if (!forward && i == 0) break;
  }
  message_ = "not found: " + query_;
  return false;
}

bool Pager::handle(const term::Key& k, std::size_t rows) {
  message_.clear();
  const auto page = static_cast<std::ptrdiff_t>(body(rows));
  // Where a repeated search starts: after (before) the last hit while it is
  // on screen, else from the top line.
  const bool shown = match_ != npos && match_ >= top_ && match_ < top_ + body(rows);
  if (typing_) {
    switch (k.code) {
      case C::Char: term::append_utf8(input_, k.ch); break;
      case C::Backspace:
        if (input_.empty()) typing_ = false;
        pop_utf8(input_);
        break;
      case C::Esc: typing_ = false; break;
      case C::Ctrl:
        if (k.ch == 'c' || k.ch == 'g') typing_ = false;
        if (k.ch == 'u') input_.clear();
        break;
      case C::Enter:
        typing_ = false;
        if (!input_.empty()) {
          query_ = fold(input_);
          search(top_, true, rows);
        } else if (!query_.empty()) {
          search(shown ? match_ + 1 : top_, true, rows);
        }
        break;
      default: break;
    }
    scroll(0, rows);
    return true;
  }

  char32_t c = k.code == C::Char ? k.ch : 0;
  if (k.code == C::Ctrl) {
    switch (k.ch) {
      case 'c': return false;
      case 'f': case 'v': c = ' '; break;
      case 'b': c = 'b'; break;
      case 'd': c = 'd'; break;
      case 'u': c = 'u'; break;
      case 'n': case 'e': case 'j': c = 'j'; break;
      case 'p': case 'y': case 'k': c = 'k'; break;
      default: break;
    }
  }
  switch (k.code) {
    case C::Esc: return false;
    case C::PageDown: c = ' '; break;
    case C::PageUp: c = 'b'; break;
    case C::Down: case C::Enter: c = 'j'; break;
    case C::Up: c = 'k'; break;
    case C::Home: c = 'g'; break;
    case C::End: c = 'G'; break;
    default: break;
  }
  switch (c) {
    case 'q': case 'Q': return false;
    case ' ': case 'f': scroll(page, rows); break;
    case 'b': scroll(-page, rows); break;
    case 'd': scroll(page / 2, rows); break;
    case 'u': scroll(-page / 2, rows); break;
    case 'j': case 'e': scroll(1, rows); break;
    case 'k': case 'y': scroll(-1, rows); break;
    case 'g': case '<': top_ = 0; break;
    case 'G': case '>': top_ = max_top(rows); break;
    case '/':
      typing_ = true;
      input_.clear();
      break;
    case 'n':
      if (query_.empty()) break;
      if (!search(shown ? match_ + 1 : top_, true, rows)) message_ = "no more matches for " + query_;
      break;
    case 'N':
      if (query_.empty()) break;
      if (shown ? match_ == 0 : top_ == 0) {
        message_ = "no more matches for " + query_;
      } else if (!search(shown ? match_ - 1 : top_ - 1, false, rows)) {
        message_ = "no more matches for " + query_;
      }
      break;
    default: scroll(0, rows); break;  // Resize and unknown keys: keep top in range
  }
  return true;
}

// One text line as drawn: with search hits in reverse video (on the plain
// text, so a hit is never split by a color code), else as painted.
std::string Pager::row(std::size_t line) const {
  if (query_.empty() || folded_[line].find(query_) == std::string::npos) return std::string(lines_[line]);
  std::string out;
  const std::string& plain = plain_[line];
  std::size_t at = 0;
  for (auto hit = folded_[line].find(query_); hit != std::string::npos;
       hit = folded_[line].find(query_, hit + query_.size())) {
    out.append(plain, at, hit - at);
    out += "\x1b[7m";
    out.append(plain, hit, query_.size());
    out += "\x1b[27m";
    at = hit + query_.size();
  }
  out.append(plain, at);
  return out;
}

std::string Pager::status(term::Size s) const {
  if (typing_) return "/" + input_;
  std::string left = message_;
  if (left.empty()) {
    const auto n = lines_.size();
    const auto last = std::min(top_ + body(s.rows), n);
    left = "lines " + std::to_string(n == 0 ? 0 : top_ + 1) + "-" + std::to_string(last) + " of " +
           std::to_string(n);
    left += top_ >= max_top(s.rows) ? "  (END)" : "  " + std::to_string(n == 0 ? 100 : last * 100 / n) + "%";
    if (!query_.empty()) left += "  /" + query_;
  }
  constexpr std::string_view help = "q quit  / search  space b page";
  // " left", at least two spaces, "help ".
  const auto lw = render::display_width(left) + 1, rw = help.size() + 1;
  if (lw + 2 + rw > s.cols) return " " + left;
  return " " + left + std::string(s.cols - lw - rw, ' ') + std::string(help) + " ";
}

std::string Pager::frame(term::Size s) const {
  std::string out;
  const auto b = body(s.rows);
  for (std::size_t r = 0; r < b; ++r) {
    out += move_to(r + 1, 1);
    if (top_ + r < lines_.size()) out += row(top_ + r);
    out += "\x1b[0m\x1b[K";
  }
  if (s.rows < 2) return out;
  const std::string st = status(s);
  const auto w = render::display_width(st);
  out += move_to(s.rows, 1) + "\x1b[7m" + st;
  if (w < s.cols) out.append(s.cols - w, ' ');
  out += "\x1b[0m";
  // The cursor shows only while a search is typed.
  out += typing_ ? move_to(s.rows, std::min(w + 1, s.cols)) + "\x1b[?25h" : std::string("\x1b[?25l");
  return out;
}

void page(term::Tty& tty, std::string_view text) {
  Pager p(text);
  for (;;) {
    const term::Size s = tty.size();
    tty.write(p.frame(s));
    const term::Key k = tty.read();
    if (k.code == C::Eof || !p.handle(k, tty.size().rows)) return;
  }
}

}  // namespace cxxman::tui
