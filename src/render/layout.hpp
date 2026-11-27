// Text measuring and styled lines (PLAN §8.5). All alignment uses display
// width, never byte length.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cxxman::render {

// Semantic roles (PLAN §4.2); themes map them to colors.
enum class Role : std::uint8_t {
  Plain, Title, Header, Badge, Sig, Note,
  Kw, Str, Num, Type,
  Ub, Warn, Perf, Link, Newer, Accent,
};

// Columns a UTF-8 string takes: East Asian wide = 2, combining marks = 0,
// everything else (box drawing and symbols included) = 1.
std::size_t display_width(std::string_view utf8);

// Splits at spaces so every line fits `width` columns; a word longer than
// the width is cut.
std::vector<std::string> wrap(std::string_view text, std::size_t width);

struct Span {
  Role role = Role::Plain;
  std::string text;
};

class Line {
 public:
  Line& add(std::string_view text, Role role = Role::Plain);
  Line& append(const Line& other);
  Line& pad_to(std::size_t column);  // spaces up to `column`; no-op if past it
  std::size_t width() const noexcept { return width_; }
  bool empty() const noexcept { return spans_.empty(); }
  const std::vector<Span>& spans() const noexcept { return spans_; }

 private:
  std::vector<Span> spans_;
  std::size_t width_ = 0;
};

using Block = std::vector<Line>;

// `left`, then `right` ending at column `width`, at least `gap` spaces apart.
// Returns false (and leaves `left` alone) when they do not fit on one line.
bool put_right(Line& left, const Line& right, std::size_t width, std::size_t gap = 2);

// Words of `text` (split at spaces), each a one-span line in `role`.
std::vector<Line> words(std::string_view text, Role role = Role::Plain);

// Greedy fill: `prefix`, then `items` separated by `space` spaces; lines
// after the first start with `hang` spaces. An item wider than a line is
// placed alone and may overflow.
Block flow(const Line& prefix, const std::vector<Line>& items, std::size_t width,
           std::size_t hang, std::size_t space = 1);

}  // namespace cxxman::render
