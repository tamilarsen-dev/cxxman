// The system clipboard (PLAN §4.9).
#pragma once

#include <string>
#include <string_view>

namespace cxxman::sys {

// Copies UTF-8 text with the system's own clipboard: OpenClipboard +
// CF_UNICODETEXT on Windows, `pbcopy` on macOS, `wl-copy`, `xclip
// -selection clipboard` or `xsel -b` elsewhere. False when none worked; the
// caller may then fall back to osc52().
bool copy(std::string_view text);

// The OSC 52 sequence that asks the terminal to put `text` on the clipboard.
std::string osc52(std::string_view text);

std::string base64(std::string_view bytes);

}  // namespace cxxman::sys
