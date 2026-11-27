#include "render/highlight.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

namespace cxxman::render {

namespace {

// C++23 keywords and alternative tokens.
constexpr std::string_view keywords[] = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool",
    "break", "case", "catch", "char", "char16_t", "char32_t", "char8_t", "class",
    "co_await", "co_return", "co_yield", "compl", "concept", "const", "const_cast",
    "consteval", "constexpr", "constinit", "continue", "decltype", "default", "delete",
    "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern",
    "false", "final", "float", "for", "friend", "goto", "if", "import", "inline",
    "int", "long", "module", "mutable", "namespace", "new", "noexcept", "not",
    "not_eq", "nullptr", "operator", "or", "or_eq", "override", "private", "protected",
    "public", "register", "reinterpret_cast", "requires", "return", "short", "signed",
    "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "template",
    "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
    "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while",
    "xor", "xor_eq",
};

bool is_keyword(std::string_view word) {
  return std::ranges::find(keywords, word) != std::end(keywords);
}

bool ident_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
         static_cast<unsigned char>(c) >= 0x80;
}
bool ident_char(char c) { return ident_start(c) || (c >= '0' && c <= '9'); }
bool digit(char c) { return c >= '0' && c <= '9'; }

class Lexer {
 public:
  explicit Lexer(std::string_view s) : s_(s), roles_(s.size(), Role::Plain) {}

  std::vector<Role> run() {
    bool line_start = true;
    while (i_ < s_.size()) {
      const char c = s_[i_];
      if (c == '\n') {
        line_start = true;
        ++i_;
        continue;
      }
      if (c == ' ') {
        ++i_;
        continue;
      }
      if (line_start && c == '#') {
        mark_until(s_.find('\n', i_), Role::Kw);
        continue;
      }
      line_start = false;
      if (starts("//")) {
        mark_until(s_.find('\n', i_), Role::Note);
      } else if (starts("/*")) {
        auto end = s_.find("*/", i_ + 2);
        mark_until(end == std::string_view::npos ? end : end + 2, Role::Note);
      } else if (raw_string()) {
      } else if (c == '"' || c == '\'') {
        quoted(c);
      } else if (digit(c) || (c == '.' && i_ + 1 < s_.size() && digit(s_[i_ + 1]))) {
        number();
      } else if (ident_start(c)) {
        identifier();
      } else {
        ++i_;
      }
    }
    return std::move(roles_);
  }

 private:
  bool starts(std::string_view p) const { return s_.substr(i_).starts_with(p); }

  void mark(std::size_t from, std::size_t to, Role r) {
    to = std::min(to, s_.size());
    std::fill(roles_.begin() + static_cast<std::ptrdiff_t>(from),
              roles_.begin() + static_cast<std::ptrdiff_t>(to), r);
  }

  void mark_until(std::size_t end, Role r) {
    if (end == std::string_view::npos) end = s_.size();
    mark(i_, end, r);
    i_ = end;
  }

  // Encoding prefixes (u8, u, U, L) are part of the literal.
  std::size_t prefix_len() const {
    for (std::string_view p : {"u8", "u", "U", "L"})
      if (starts(p)) return p.size();
    return 0;
  }

  bool raw_string() {
    const std::size_t pre = prefix_len();
    if (!s_.substr(i_ + pre).starts_with("R\"")) return false;
    const std::size_t open = i_ + pre + 2;
    const auto paren = s_.find('(', open);
    if (paren == std::string_view::npos || paren - open > 16) return false;
    const std::string close = ")" + std::string(s_.substr(open, paren - open)) + "\"";
    auto end = s_.find(close, paren + 1);
    mark_until(end == std::string_view::npos ? end : end + close.size(), Role::Str);
    return true;
  }

  void quoted(char q) {
    std::size_t j = i_ + 1;
    while (j < s_.size() && s_[j] != q && s_[j] != '\n') j += s_[j] == '\\' ? 2 : 1;
    mark_until(std::min(j + 1, s_.size()), Role::Str);
  }

  void number() {
    std::size_t j = i_;
    while (j < s_.size()) {
      const char c = s_[j];
      if (ident_char(c) || c == '.' || (c == '\'' && j + 1 < s_.size() && ident_char(s_[j + 1]))) {
        ++j;
      } else if ((c == '+' || c == '-') && j > i_ && (s_[j - 1] == 'e' || s_[j - 1] == 'E' ||
                                                      s_[j - 1] == 'p' || s_[j - 1] == 'P')) {
        ++j;
      } else {
        break;
      }
    }
    mark_until(j, Role::Num);
  }

  void identifier() {
    // A literal with an encoding prefix: u8"..", L'x'.
    const std::size_t pre = prefix_len();
    if (pre > 0 && i_ + pre < s_.size() && (s_[i_ + pre] == '"' || s_[i_ + pre] == '\'')) {
      const std::size_t start = i_;
      i_ += pre;
      quoted(s_[i_]);
      mark(start, i_, Role::Str);
      return;
    }
    std::size_t j = i_;
    while (j < s_.size() && ident_char(s_[j])) ++j;
    const std::string_view word = s_.substr(i_, j - i_);
    if (word == "std" && s_.substr(j).starts_with("::")) {
      // std::ranges::sort: the whole qualified name is a type-like name.
      while (s_.substr(j).starts_with("::") && j + 2 < s_.size() && ident_start(s_[j + 2])) {
        j += 2;
        while (j < s_.size() && ident_char(s_[j])) ++j;
      }
      mark_until(j, Role::Type);
      return;
    }
    mark_until(j, is_keyword(word) ? Role::Kw : Role::Plain);
  }

  std::string_view s_;
  std::vector<Role> roles_;
  std::size_t i_ = 0;
};

}  // namespace

std::vector<Role> highlight(std::string_view code) { return Lexer(code).run(); }

}  // namespace cxxman::render
