#include "term/keys.hpp"

namespace cxxman::term {

namespace {

using C = Key::Code;

// The key a CSI sequence (ESC [ params final) stands for.
Key csi_key(std::string_view params, char final) {
  switch (final) {
    case 'A': return key(C::Up);
    case 'B': return key(C::Down);
    case 'C': return key(C::Right);
    case 'D': return key(C::Left);
    case 'H': return key(C::Home);
    case 'F': return key(C::End);
    case '~': break;
    default: return key(C::None);
  }
  // ESC [ n ~ and ESC [ n ; mods ~: the first number names the key.
  int n = 0;
  for (char c : params) {
    if (c < '0' || c > '9') break;
    n = n * 10 + (c - '0');
    if (n > 99) return key(C::None);
  }
  switch (n) {
    case 1: case 7: return key(C::Home);
    case 4: case 8: return key(C::End);
    case 3: return key(C::Delete);
    case 5: return key(C::PageUp);
    case 6: return key(C::PageDown);
    default: return key(C::None);
  }
}

Key ss3_key(char c) {
  switch (c) {
    case 'A': return key(C::Up);
    case 'B': return key(C::Down);
    case 'C': return key(C::Right);
    case 'D': return key(C::Left);
    case 'H': return key(C::Home);
    case 'F': return key(C::End);
    default: return key(C::None);
  }
}

Decoded escape(std::string_view b, bool final) {
  if (b.size() == 1) return final ? Decoded{key(C::Esc), 1} : Decoded{};
  if (b[1] == 'O') {
    if (b.size() == 2) return final ? Decoded{key(C::Esc), 1} : Decoded{};
    return {ss3_key(b[2]), 3};
  }
  if (b[1] != '[') return {key(C::Esc), 1};  // Esc, then the next key (Alt+x)
  // Parameter and intermediate bytes (0x20-0x3F), then one final byte.
  for (std::size_t i = 2; i < b.size(); ++i) {
    const auto c = static_cast<unsigned char>(b[i]);
    if (c >= 0x40 && c <= 0x7E) return {csi_key(b.substr(2, i - 2), b[i]), i + 1};
    if (c < 0x20 || c > 0x3F) return {key(C::None), i};  // broken: drop what we read
  }
  return final ? Decoded{key(C::Esc), 1} : Decoded{};
}

Decoded utf8(std::string_view b, bool final) {
  const auto lead = static_cast<unsigned char>(b[0]);
  std::size_t len = 0;
  char32_t cp = 0;
  if (lead >= 0xC2 && lead <= 0xDF) {
    len = 2;
    cp = lead & 0x1Fu;
  } else if (lead >= 0xE0 && lead <= 0xEF) {
    len = 3;
    cp = lead & 0x0Fu;
  } else if (lead >= 0xF0 && lead <= 0xF4) {
    len = 4;
    cp = lead & 0x07u;
  } else {
    return {key(C::None), 1};
  }
  for (std::size_t i = 1; i < len; ++i) {
    if (i >= b.size()) return final ? Decoded{key(C::None), b.size()} : Decoded{};
    const auto c = static_cast<unsigned char>(b[i]);
    if ((c & 0xC0u) != 0x80u) return {key(C::None), i};
    cp = (cp << 6) | (c & 0x3Fu);
  }
  const bool overlong = (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000);
  if (overlong || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return {key(C::None), len};
  return {chr(cp), len};
}

}  // namespace

Decoded decode(std::string_view b, bool final) {
  if (b.empty()) return {};
  const auto c = static_cast<unsigned char>(b[0]);
  switch (c) {
    case 0x1B: return escape(b, final);
    case '\r': case '\n': return {key(C::Enter), 1};
    case '\t': return {key(C::Tab), 1};
    case 0x7F: case 0x08: return {key(C::Backspace), 1};
    default: break;
  }
  if (c >= 0x01 && c <= 0x1A) return {ctrl(static_cast<char>('a' + c - 1)), 1};
  if (c < 0x20) return {key(C::None), 1};
  if (c < 0x80) return {chr(c), 1};
  return utf8(b, final);
}

void append_utf8(std::string& out, char32_t c) {
  auto put = [&](unsigned v) { out += static_cast<char>(v); };
  if (c < 0x80) {
    put(static_cast<unsigned>(c));
  } else if (c < 0x800) {
    put(0xC0u | static_cast<unsigned>(c >> 6));
    put(0x80u | (static_cast<unsigned>(c) & 0x3Fu));
  } else if (c < 0x10000) {
    put(0xE0u | static_cast<unsigned>(c >> 12));
    put(0x80u | (static_cast<unsigned>(c >> 6) & 0x3Fu));
    put(0x80u | (static_cast<unsigned>(c) & 0x3Fu));
  } else {
    put(0xF0u | static_cast<unsigned>(c >> 18));
    put(0x80u | (static_cast<unsigned>(c >> 12) & 0x3Fu));
    put(0x80u | (static_cast<unsigned>(c >> 6) & 0x3Fu));
    put(0x80u | (static_cast<unsigned>(c) & 0x3Fu));
  }
}

}  // namespace cxxman::term
