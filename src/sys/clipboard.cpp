#include "sys/clipboard.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#else
#include "sys/process.hpp"
#endif

#include <cstdint>

namespace cxxman::sys {

std::string base64(std::string_view bytes) {
  constexpr char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve((bytes.size() + 2) / 3 * 4);
  for (std::size_t i = 0; i < bytes.size(); i += 3) {
    const std::size_t left = bytes.size() - i;
    std::uint32_t v = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i])) << 16;
    if (left > 1) v |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i + 1])) << 8;
    if (left > 2) v |= static_cast<unsigned char>(bytes[i + 2]);
    out += digits[(v >> 18) & 63];
    out += digits[(v >> 12) & 63];
    out += left > 1 ? digits[(v >> 6) & 63] : '=';
    out += left > 2 ? digits[v & 63] : '=';
  }
  return out;
}

std::string osc52(std::string_view text) { return "\x1b]52;c;" + base64(text) + "\x07"; }

#ifdef _WIN32

bool copy(std::string_view text) {
  // Windows text uses CRLF line ends.
  std::string crlf;
  for (char c : text) {
    if (c == '\n') crlf += '\r';
    crlf += c;
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, crlf.data(), static_cast<int>(crlf.size()), nullptr, 0);
  if (n <= 0 || !OpenClipboard(nullptr)) return false;
  bool ok = false;
  if (EmptyClipboard()) {
    const std::size_t bytes = (static_cast<std::size_t>(n) + 1) * sizeof(wchar_t);
    if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
      if (auto* p = static_cast<wchar_t*>(GlobalLock(mem))) {
        MultiByteToWideChar(CP_UTF8, 0, crlf.data(), static_cast<int>(crlf.size()), p, n);
        p[n] = L'\0';
        GlobalUnlock(mem);
        ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
      }
      if (!ok) GlobalFree(mem);  // on success the clipboard owns it
    }
  }
  CloseClipboard();
  return ok;
}

#else

bool copy(std::string_view text) {
#ifdef __APPLE__
  const std::string pbcopy[] = {"pbcopy"};
  return run(pbcopy, text) == 0;
#else
  const std::string wl[] = {"wl-copy"};
  const std::string xclip[] = {"xclip", "-selection", "clipboard"};
  const std::string xsel[] = {"xsel", "-b"};
  return run(wl, text) == 0 || run(xclip, text) == 0 || run(xsel, text) == 0;
#endif
}

#endif

}  // namespace cxxman::sys
