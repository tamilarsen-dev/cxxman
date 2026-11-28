// Keys read from a terminal, and the decoder that turns the bytes a POSIX
// terminal sends into them (PLAN §4.6, §4.8). Platform-free, so the tests can
// feed it escape sequences; the finder (M5) uses the same keys.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace cxxman::term {

struct Key {
  enum class Code : std::uint8_t {
    None,       // nothing usable (an unknown escape sequence)
    Char,       // a printable character: ch
    Ctrl,       // Ctrl + a letter: ch is the lowercase letter ('c' for Ctrl-C)
    Enter, Esc, Backspace, Tab, Delete,
    Up, Down, Left, Right, PageUp, PageDown, Home, End,
    Resize,     // the window changed size
    Eof,        // the input went away
  };
  Code code = Code::None;
  char32_t ch = 0;

  friend bool operator==(const Key&, const Key&) = default;
};

constexpr Key key(Key::Code c) { return {c, 0}; }
constexpr Key chr(char32_t c) { return {Key::Code::Char, c}; }
constexpr Key ctrl(char c) { return {Key::Code::Ctrl, static_cast<char32_t>(c)}; }

struct Decoded {
  Key key;
  std::size_t used = 0;  // bytes consumed; 0: need more bytes
};

// Decodes the first key in `bytes`. When `bytes` could be the start of a
// longer sequence (a lone ESC, half a UTF-8 character) it returns used = 0,
// unless `final` says no more bytes are coming: then a lone ESC is Esc and a
// broken sequence is skipped. An empty `bytes` returns used = 0.
Decoded decode(std::string_view bytes, bool final);

// Appends `c` as UTF-8.
void append_utf8(std::string& out, char32_t c);

}  // namespace cxxman::term
