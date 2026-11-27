#include "render/layout.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace cxxman::render {

namespace {

// Decodes one code point; invalid bytes count as one replacement character.
char32_t next(std::string_view s, std::size_t& i) {
  const auto b = static_cast<unsigned char>(s[i]);
  const int extra = b < 0x80 ? 0 : (b >> 5) == 0x6 ? 1 : (b >> 4) == 0xE ? 2 : (b >> 3) == 0x1E ? 3 : -1;
  if (extra < 0 || i + static_cast<std::size_t>(extra) >= s.size()) {
    ++i;
    return 0xFFFD;
  }
  char32_t cp = extra == 0 ? b : b & (0x3F >> extra);
  for (int k = 1; k <= extra; ++k) {
    const auto c = static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]);
    if ((c & 0xC0) != 0x80) {
      ++i;
      return 0xFFFD;
    }
    cp = (cp << 6) | (c & 0x3F);
  }
  i += static_cast<std::size_t>(extra) + 1;
  return cp;
}

struct Range {
  char32_t first, last;
};

constexpr std::array<Range, 5> zero_width{{
    {0x0300, 0x036F}, {0x200B, 0x200F}, {0x20D0, 0x20FF}, {0xFE00, 0xFE0F}, {0xFE20, 0xFE2F},
}};

constexpr std::array<Range, 12> wide{{
    {0x1100, 0x115F}, {0x2E80, 0x303E}, {0x3041, 0x33FF}, {0x3400, 0x4DBF},
    {0x4E00, 0x9FFF}, {0xA000, 0xA4CF}, {0xAC00, 0xD7A3}, {0xF900, 0xFAFF},
    {0xFE30, 0xFE4F}, {0xFF00, 0xFF60}, {0xFFE0, 0xFFE6}, {0x1F300, 0x3FFFD},
}};

bool in(const auto& table, char32_t cp) {
  return std::ranges::any_of(table, [cp](Range r) { return cp >= r.first && cp <= r.last; });
}

std::size_t width_of(char32_t cp) {
  if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0) || in(zero_width, cp)) return 0;
  return in(wide, cp) ? 2 : 1;
}

}  // namespace

std::size_t display_width(std::string_view s) {
  std::size_t w = 0;
  for (std::size_t i = 0; i < s.size();) w += width_of(next(s, i));
  return w;
}

std::vector<std::string> wrap(std::string_view text, std::size_t width) {
  std::vector<std::string> lines;
  std::string line;
  std::size_t line_w = 0;
  auto flush = [&] {
    lines.push_back(std::move(line));
    line.clear();
    line_w = 0;
  };
  std::size_t pos = 0;
  while (pos < text.size()) {
    if (text[pos] == ' ') {
      ++pos;
      continue;
    }
    auto end = text.find(' ', pos);
    if (end == std::string_view::npos) end = text.size();
    std::string_view word = text.substr(pos, end - pos);
    pos = end;
    std::size_t word_w = display_width(word);
    if (line_w > 0 && line_w + 1 + word_w <= width) {
      line += ' ';
      line += word;
      line_w += 1 + word_w;
      continue;
    }
    if (line_w > 0) flush();
    // Cut words that are wider than a whole line.
    while (word_w > width) {
      std::size_t i = 0, w = 0;
      while (i < word.size()) {
        std::size_t j = i;
        auto cw = width_of(next(word, j));
        if (w + cw > width) break;
        w += cw;
        i = j;
      }
      lines.emplace_back(word.substr(0, i));
      word.remove_prefix(i);
      word_w = display_width(word);
    }
    line = word;
    line_w = word_w;
  }
  if (line_w > 0 || lines.empty()) flush();
  return lines;
}

std::string truncate(std::string_view text, std::size_t width, std::string_view ellipsis) {
  if (display_width(text) <= width) return std::string(text);
  const std::size_t room = width > display_width(ellipsis) ? width - display_width(ellipsis) : 0;
  std::size_t i = 0, w = 0;
  while (i < text.size()) {
    std::size_t j = i;
    const auto cw = width_of(next(text, j));
    if (w + cw > room) break;
    w += cw;
    i = j;
  }
  // Do not leave a space before the ellipsis.
  while (i > 0 && text[i - 1] == ' ') --i;
  return std::string(text.substr(0, i)) + std::string(ellipsis);
}

Line& Line::add(std::string_view text, Role role) {
  if (text.empty()) return *this;
  if (!spans_.empty() && spans_.back().role == role)
    spans_.back().text += text;
  else
    spans_.push_back({role, std::string(text)});
  width_ += display_width(text);
  return *this;
}

Line& Line::append(const Line& other) {
  for (const auto& s : other.spans()) add(s.text, s.role);
  return *this;
}

Line& Line::pad_to(std::size_t column) {
  if (width_ < column) add(std::string(column - width_, ' '));
  return *this;
}

Line spaces(std::size_t n) {
  Line l;
  l.add(std::string(n, ' '));
  return l;
}

std::string group_digits(std::size_t n) {
  std::string digits = std::to_string(n), out;
  for (std::size_t i = 0; i < digits.size(); ++i) {
    if (i > 0 && (digits.size() - i) % 3 == 0) out += ',';
    out += digits[i];
  }
  return out;
}

bool put_right(Line& left, const Line& right, std::size_t width, std::size_t gap) {
  if (left.width() + gap + right.width() > width) return false;
  left.pad_to(width - right.width());
  left.append(right);
  return true;
}

std::vector<Line> words(std::string_view text, Role role) {
  std::vector<Line> out;
  std::size_t pos = 0;
  while (pos < text.size()) {
    if (text[pos] == ' ') {
      ++pos;
      continue;
    }
    auto end = text.find(' ', pos);
    if (end == std::string_view::npos) end = text.size();
    out.emplace_back().add(text.substr(pos, end - pos), role);
    pos = end;
  }
  return out;
}

Block flow(const Line& prefix, const std::vector<Line>& items, std::size_t width,
           std::size_t hang, std::size_t space) {
  Block out;
  Line cur = prefix;
  bool fresh = true;  // no item on `cur` yet
  for (const auto& item : items) {
    if (!fresh && cur.width() + space + item.width() > width) {
      out.push_back(std::move(cur));
      cur = Line{};
      cur.add(std::string(hang, ' '));
      fresh = true;
    }
    if (!fresh) {
      // A space between two items of one role takes that role, so the spans
      // merge and the color is not switched off and on per word. Not for
      // links: an underlined space shows.
      const Role prev = cur.spans().back().role;
      const bool same = !item.empty() && item.spans().front().role == prev;
      cur.add(std::string(space, ' '), same && prev != Role::Link ? prev : Role::Plain);
    }
    cur.append(item);
    fresh = false;
  }
  out.push_back(std::move(cur));
  return out;
}

}  // namespace cxxman::render
