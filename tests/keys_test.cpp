// Key decoding: the bytes a terminal sends -> keys (PLAN §10).
#include "term/keys.hpp"

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <vector>

using namespace cxxman::term;
using C = Key::Code;

namespace {

// Every key in `bytes`, decoding as if no more bytes come.
std::vector<Key> all(std::string_view bytes) {
  std::vector<Key> out;
  while (!bytes.empty()) {
    const Decoded d = decode(bytes, true);
    REQUIRE(d.used > 0);
    out.push_back(d.key);
    bytes.remove_prefix(d.used);
  }
  return out;
}

Key one(std::string_view bytes) {
  const Decoded d = decode(bytes, false);
  CHECK(d.used == bytes.size());
  return d.key;
}

}  // namespace

TEST_CASE("plain bytes become characters, control keys and Ctrl+letter") {
  CHECK(one("q") == chr('q'));
  CHECK(one(" ") == chr(' '));
  CHECK(one("/") == chr('/'));
  CHECK(one("\r") == key(C::Enter));
  CHECK(one("\n") == key(C::Enter));
  CHECK(one("\t") == key(C::Tab));
  CHECK(one("\x7f") == key(C::Backspace));
  CHECK(one("\x08") == key(C::Backspace));
  CHECK(one("\x03") == ctrl('c'));
  CHECK(one("\x01") == ctrl('a'));
  CHECK(one("\x1a") == ctrl('z'));
  CHECK(one(std::string_view("\0", 1)) == key(C::None));
}

TEST_CASE("CSI and SS3 sequences become arrows and paging keys") {
  CHECK(one("\x1b[A") == key(C::Up));
  CHECK(one("\x1b[B") == key(C::Down));
  CHECK(one("\x1b[C") == key(C::Right));
  CHECK(one("\x1b[D") == key(C::Left));
  CHECK(one("\x1bOA") == key(C::Up));  // application cursor mode
  CHECK(one("\x1bOB") == key(C::Down));
  CHECK(one("\x1b[H") == key(C::Home));
  CHECK(one("\x1b[F") == key(C::End));
  CHECK(one("\x1bOH") == key(C::Home));
  CHECK(one("\x1bOF") == key(C::End));
  CHECK(one("\x1b[1~") == key(C::Home));
  CHECK(one("\x1b[7~") == key(C::Home));
  CHECK(one("\x1b[4~") == key(C::End));
  CHECK(one("\x1b[8~") == key(C::End));
  CHECK(one("\x1b[3~") == key(C::Delete));
  CHECK(one("\x1b[5~") == key(C::PageUp));
  CHECK(one("\x1b[6~") == key(C::PageDown));
  CHECK(one("\x1b[1;5A") == key(C::Up));     // Ctrl+Up: the modifier is ignored
  CHECK(one("\x1b[5;2~") == key(C::PageUp));  // Shift+PgUp
  CHECK(one("\x1b[2~") == key(C::None));      // Insert: consumed, unused
  CHECK(one("\x1b[Z") == key(C::None));       // Shift+Tab
  CHECK(one("\x1b[200~") == key(C::None));    // bracketed paste start
}

TEST_CASE("a lone ESC waits for more bytes, then is Esc") {
  CHECK(decode("\x1b", false).used == 0);
  CHECK(decode("\x1b[", false).used == 0);
  CHECK(decode("\x1b[1;5", false).used == 0);
  CHECK(decode("\x1bO", false).used == 0);
  const Decoded d = decode("\x1b", true);
  CHECK(d.key == key(C::Esc));
  CHECK(d.used == 1);
  // Esc, then '[' typed by hand: the '[' is not lost.
  CHECK(all("\x1b[") == std::vector<Key>{key(C::Esc), chr('[')});
  // Alt+x arrives as ESC x: Esc, then x.
  CHECK(all("\x1bx") == std::vector<Key>{key(C::Esc), chr('x')});
  CHECK(all("\x1b\x1b[A") == std::vector<Key>{key(C::Esc), key(C::Up)});
  CHECK(decode("", true).used == 0);
}

TEST_CASE("several keys in one read decode one after another") {
  CHECK(all("j\x1b[6~q") == std::vector<Key>{chr('j'), key(C::PageDown), chr('q')});
  CHECK(all("/so\r") == std::vector<Key>{chr('/'), chr('s'), chr('o'), key(C::Enter)});
}

TEST_CASE("UTF-8 characters decode whole, broken ones are skipped") {
  CHECK(one("\xC3\xA9") == chr(U'é'));          // é
  CHECK(one("\xE4\xB8\xAD") == chr(U'中'));      // 中
  CHECK(one("\xF0\x9F\x98\x80") == chr(U'\U0001F600'));
  CHECK(decode("\xE4\xB8", false).used == 0);         // half a character: wait
  CHECK(decode("\xE4\xB8", true).key == key(C::None));
  CHECK(decode("\xE4\xB8", true).used == 2);
  CHECK(decode("\x80", true).key == key(C::None));    // a lone continuation byte
  CHECK(decode("\xC0\xAF", true).key == key(C::None));  // overlong
  CHECK(all("\xC3x") == std::vector<Key>{key(C::None), chr('x')});
}

TEST_CASE("append_utf8 writes 1 to 4 bytes") {
  std::string s;
  append_utf8(s, U'a');
  append_utf8(s, U'é');
  append_utf8(s, U'中');
  append_utf8(s, U'\U0001F600');
  CHECK(s == "a\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80");
}
