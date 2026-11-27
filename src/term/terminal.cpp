#include "term/terminal.hpp"

#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <string_view>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace cxxman::term {

#ifdef _WIN32

namespace {

bool is_console(DWORD which) {
  DWORD mode = 0;
  return GetConsoleMode(GetStdHandle(which), &mode) != 0;
}

bool write_all(DWORD which, std::string_view s) {
  HANDLE h = GetStdHandle(which);
  while (!s.empty()) {
    DWORD written = 0;
    const auto chunk = static_cast<DWORD>(std::min<std::size_t>(s.size(), 1u << 20));
    if (!WriteFile(h, s.data(), chunk, &written, nullptr)) return false;
    s.remove_prefix(written);
  }
  return true;
}

}  // namespace

Console::Console() {
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  info_.out_tty = GetConsoleMode(out, &mode) != 0;
  info_.err_tty = is_console(STD_ERROR_HANDLE);
  info_.in_tty = is_console(STD_INPUT_HANDLE);
  if (!info_.out_tty) return;

  old_mode_ = mode;
  restore_mode_ = true;
  info_.vt = (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0 ||
             SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(out, &csbi))
    info_.columns = static_cast<std::size_t>(csbi.srWindow.Right - csbi.srWindow.Left + 1);

  old_cp_ = GetConsoleOutputCP();
  info_.utf8 = old_cp_ == CP_UTF8 || SetConsoleOutputCP(CP_UTF8) != 0;
}

Console::~Console() {
  if (restore_mode_) SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), old_mode_);
  if (old_cp_ != 0 && old_cp_ != CP_UTF8) SetConsoleOutputCP(old_cp_);
}

bool write_out(std::string_view s) { return write_all(STD_OUTPUT_HANDLE, s); }
bool write_err(std::string_view s) { return write_all(STD_ERROR_HANDLE, s); }

#else

namespace {

// The first of LC_ALL, LC_CTYPE and LANG that is set decides; none set
// counts as UTF-8, which is what terminals use today.
bool locale_is_utf8() {
  for (const char* name : {"LC_ALL", "LC_CTYPE", "LANG"}) {
    const char* v = std::getenv(name);
    if (!v || !*v) continue;
    std::string_view s = v;
    for (std::string_view tag : {"UTF-8", "utf-8", "UTF8", "utf8"})
      if (s.find(tag) != std::string_view::npos) return true;
    return false;
  }
  return true;
}

bool write_all(int fd, std::string_view s) {
  while (!s.empty()) {
    const auto n = ::write(fd, s.data(), s.size());
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    s.remove_prefix(static_cast<std::size_t>(n));
  }
  return true;
}

}  // namespace

Console::Console() {
  std::signal(SIGPIPE, SIG_IGN);  // a closed pipe becomes EPIPE from write()
  info_.out_tty = ::isatty(STDOUT_FILENO) != 0;
  info_.err_tty = ::isatty(STDERR_FILENO) != 0;
  info_.in_tty = ::isatty(STDIN_FILENO) != 0;
  info_.vt = true;
  if (!info_.out_tty) return;
  winsize ws{};
  if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) info_.columns = ws.ws_col;
  info_.utf8 = locale_is_utf8();
}

Console::~Console() = default;

bool write_out(std::string_view s) { return write_all(STDOUT_FILENO, s); }
bool write_err(std::string_view s) { return write_all(STDERR_FILENO, s); }

#endif

}  // namespace cxxman::term
