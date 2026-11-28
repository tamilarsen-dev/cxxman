#include "term/tty.hpp"

#include <algorithm>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <csignal>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace cxxman::term {

namespace {

// Alternate screen on, cursor hidden, line wrap off; and back.
constexpr std::string_view enter_seq = "\x1b[?1049h\x1b[?25l\x1b[?7l";
constexpr std::string_view leave_seq = "\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l";

using C = Key::Code;

}  // namespace

#ifdef _WIN32

namespace {

// What restore() puts back. A console control handler runs on another
// thread, hence the atomic.
HANDLE g_in = INVALID_HANDLE_VALUE;
DWORD g_in_mode = 0;
std::atomic<bool> g_active{false};
LPTOP_LEVEL_EXCEPTION_FILTER g_old_filter = nullptr;
void (*g_old_abort)(int) = SIG_DFL;

void write_all(HANDLE h, std::string_view s) {
  while (!s.empty()) {
    DWORD n = 0;
    if (!WriteFile(h, s.data(), static_cast<DWORD>(std::min<std::size_t>(s.size(), 1u << 20)), &n, nullptr) ||
        n == 0)
      return;
    s.remove_prefix(n);
  }
}

// Safe to call twice and from any thread.
void restore() {
  if (!g_active.exchange(false)) return;
  write_all(GetStdHandle(STD_OUTPUT_HANDLE), leave_seq);
  SetConsoleMode(g_in, g_in_mode);
}

// Ctrl-Break and closing the window (Ctrl-C arrives as a key).
BOOL WINAPI on_ctrl(DWORD) {
  restore();
  return FALSE;  // let the default handler end the process
}

LONG WINAPI on_crash(EXCEPTION_POINTERS*) {
  restore();
  return EXCEPTION_CONTINUE_SEARCH;
}

void on_abort(int sig) {
  restore();
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}

class WinTty final : public Tty {
 public:
  explicit WinTty(HANDLE out) : out_(out) {}
  WinTty(const WinTty&) = delete;
  WinTty& operator=(const WinTty&) = delete;

  ~WinTty() override {
    restore();
    std::signal(SIGABRT, g_old_abort);
    SetUnhandledExceptionFilter(g_old_filter);
    SetConsoleCtrlHandler(on_ctrl, FALSE);
    CloseHandle(g_in);
    g_in = INVALID_HANDLE_VALUE;
  }

  Size size() override {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(out_, &csbi)) return {24, 80};
    return {static_cast<std::size_t>(csbi.srWindow.Bottom - csbi.srWindow.Top + 1),
            static_cast<std::size_t>(csbi.srWindow.Right - csbi.srWindow.Left + 1)};
  }

  Key read() override {
    if (repeat_ > 0) {
      --repeat_;
      return repeated_;
    }
    for (;;) {
      INPUT_RECORD r;
      DWORD n = 0;
      if (!ReadConsoleInputW(g_in, &r, 1, &n)) return key(C::Eof);
      if (n == 0) continue;
      if (r.EventType == WINDOW_BUFFER_SIZE_EVENT) return key(C::Resize);
      if (r.EventType != KEY_EVENT || !r.Event.KeyEvent.bKeyDown) continue;
      const Key k = translate(r.Event.KeyEvent);
      if (k.code == C::None) continue;
      if (r.Event.KeyEvent.wRepeatCount > 1) {
        repeated_ = k;
        repeat_ = r.Event.KeyEvent.wRepeatCount - 1u;
      }
      return k;
    }
  }

  void write(std::string_view s) override { write_all(out_, s); }

 private:
  Key translate(const KEY_EVENT_RECORD& e) {
    switch (e.wVirtualKeyCode) {
      case VK_UP: return key(C::Up);
      case VK_DOWN: return key(C::Down);
      case VK_LEFT: return key(C::Left);
      case VK_RIGHT: return key(C::Right);
      case VK_PRIOR: return key(C::PageUp);
      case VK_NEXT: return key(C::PageDown);
      case VK_HOME: return key(C::Home);
      case VK_END: return key(C::End);
      case VK_DELETE: return key(C::Delete);
      case VK_RETURN: return key(C::Enter);
      case VK_ESCAPE: return key(C::Esc);
      case VK_BACK: return key(C::Backspace);
      case VK_TAB: return key(C::Tab);
      default: break;
    }
    const auto c = static_cast<char32_t>(e.uChar.UnicodeChar);
    if (c >= 0xD800 && c <= 0xDBFF) {  // the first half of a surrogate pair
      high_ = c;
      return key(C::None);
    }
    if (c >= 0xDC00 && c <= 0xDFFF) {
      const char32_t high = high_;
      high_ = 0;
      if (high == 0) return key(C::None);
      return chr(0x10000 + ((high - 0xD800) << 10) + (c - 0xDC00));
    }
    high_ = 0;
    if (c >= 1 && c <= 26) return ctrl(static_cast<char>('a' + c - 1));
    if (c < 0x20 || c == 0x7F) return key(C::None);
    return chr(c);
  }

  HANDLE out_;
  char32_t high_ = 0;
  Key repeated_;
  unsigned repeat_ = 0;
};

}  // namespace

std::unique_ptr<Tty> open_fullscreen() {
  if (g_active) return nullptr;
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD out_mode = 0;
  if (!GetConsoleMode(out, &out_mode) || !(out_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return nullptr;
  HANDLE in = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                          OPEN_EXISTING, 0, nullptr);
  if (in == INVALID_HANDLE_VALUE) return nullptr;
  DWORD in_mode = 0;
  if (!GetConsoleMode(in, &in_mode)) {
    CloseHandle(in);
    return nullptr;
  }
  g_in = in;
  g_in_mode = in_mode;
  SetConsoleCtrlHandler(on_ctrl, TRUE);
  g_old_filter = SetUnhandledExceptionFilter(on_crash);
  g_old_abort = std::signal(SIGABRT, on_abort);
  auto tty = std::make_unique<WinTty>(out);
  // Keys one at a time, no echo, Ctrl-C as a key, and resize events.
  const DWORD raw = (in_mode & ~static_cast<DWORD>(ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
                                                    ENABLE_MOUSE_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT)) |
                    ENABLE_WINDOW_INPUT;
  SetConsoleMode(in, raw);
  g_active = true;
  tty->write(enter_seq);
  return tty;
}

#else

namespace {

// What restore() puts back; signal handlers read it.
int g_fd = -1;
termios g_saved{};
volatile std::sig_atomic_t g_active = 0;
int g_wake = -1;  // the write end of the resize pipe

constexpr int fatal_signals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGABRT, SIGSEGV, SIGBUS, SIGFPE, SIGILL};
constexpr std::size_t fatal_count = sizeof(fatal_signals) / sizeof(fatal_signals[0]);
struct sigaction g_old_fatal[fatal_count];
struct sigaction g_old_winch;

void write_all(std::string_view s) {
  while (!s.empty()) {
    const auto n = ::write(STDOUT_FILENO, s.data(), s.size());
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return;
    s.remove_prefix(static_cast<std::size_t>(n));
  }
}

// Async-signal-safe; safe to call twice.
void restore() {
  if (!g_active) return;
  g_active = 0;
  write_all(leave_seq);
  ::tcsetattr(g_fd, TCSANOW, &g_saved);
}

void on_fatal(int sig) {
  restore();
  ::raise(sig);  // SA_RESETHAND: the default action, once this handler returns
}

void on_winch(int) {
  const int saved = errno;
  const char b = 1;
  [[maybe_unused]] const auto n = ::write(g_wake, &b, 1);
  errno = saved;
}

void handle(int sig, void (*fn)(int), int flags, struct sigaction* old) {
  struct sigaction sa{};
  sa.sa_handler = fn;
  sa.sa_flags = flags;
  sigemptyset(&sa.sa_mask);
  ::sigaction(sig, &sa, old);
}

class PosixTty final : public Tty {
 public:
  PosixTty(int fd, int wake_read) : fd_(fd), wake_(wake_read) {}
  PosixTty(const PosixTty&) = delete;
  PosixTty& operator=(const PosixTty&) = delete;

  ~PosixTty() override {
    restore();
    for (std::size_t i = 0; i < fatal_count; ++i) ::sigaction(fatal_signals[i], &g_old_fatal[i], nullptr);
    ::sigaction(SIGWINCH, &g_old_winch, nullptr);
    ::close(wake_);
    ::close(g_wake);
    g_wake = -1;
    ::close(fd_);
    g_fd = -1;
  }

  Size size() override {
    winsize ws{};
    if (::ioctl(fd_, TIOCGWINSZ, &ws) != 0 || ws.ws_row == 0 || ws.ws_col == 0) return {24, 80};
    return {ws.ws_row, ws.ws_col};
  }

  Key read() override {
    for (;;) {
      if (!buf_.empty()) {
        const Decoded d = decode(buf_, false);
        if (d.used > 0) {
          buf_.erase(0, d.used);
          if (d.key.code != C::None) return d.key;
          continue;
        }
      }
      // A lone ESC is Esc when nothing follows within 25 ms (PLAN §4.6).
      switch (fill(buf_.empty() ? -1 : 25)) {
        case Got::Bytes: break;
        case Got::Resize: return key(C::Resize);
        case Got::Closed: return key(C::Eof);
        case Got::Timeout: {
          const Decoded d = decode(buf_, true);
          buf_.erase(0, std::max<std::size_t>(d.used, 1));
          if (d.key.code != C::None) return d.key;
          break;
        }
      }
    }
  }

  void write(std::string_view s) override { write_all(s); }

 private:
  enum class Got { Bytes, Resize, Closed, Timeout };

  Got fill(int timeout_ms) {
    for (;;) {
      pollfd p[2] = {{fd_, POLLIN, 0}, {wake_, POLLIN, 0}};
      const int r = ::poll(p, 2, timeout_ms);
      if (r < 0) {
        if (errno == EINTR) continue;
        return Got::Closed;
      }
      if (r == 0) return Got::Timeout;
      if (p[1].revents & POLLIN) {
        char drain[64];
        while (::read(wake_, drain, sizeof drain) > 0) {
        }
        return Got::Resize;
      }
      char chunk[256];
      const auto n = ::read(fd_, chunk, sizeof chunk);
      if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
      if (n <= 0) return Got::Closed;
      buf_.append(chunk, static_cast<std::size_t>(n));
      return Got::Bytes;
    }
  }

  int fd_, wake_;
  std::string buf_;
};

}  // namespace

std::unique_ptr<Tty> open_fullscreen() {
  if (g_active || !::isatty(STDOUT_FILENO)) return nullptr;
  const int fd = ::open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
  if (fd < 0) return nullptr;
  termios t{};
  int wake[2];
  if (::tcgetattr(fd, &t) != 0 || ::pipe(wake) != 0) {
    ::close(fd);
    return nullptr;
  }
  for (int w : wake) {
    ::fcntl(w, F_SETFL, ::fcntl(w, F_GETFL) | O_NONBLOCK);
    ::fcntl(w, F_SETFD, FD_CLOEXEC);
  }
  g_fd = fd;
  g_saved = t;
  g_wake = wake[1];
  for (std::size_t i = 0; i < fatal_count; ++i) handle(fatal_signals[i], on_fatal, SA_RESETHAND, &g_old_fatal[i]);
  handle(SIGWINCH, on_winch, SA_RESTART, &g_old_winch);
  auto tty = std::make_unique<PosixTty>(fd, wake[0]);
  // Raw input: bytes as they come, no echo, Ctrl-C and Ctrl-Z as keys.
  t.c_iflag &= ~static_cast<tcflag_t>(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  t.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN | ISIG);
  t.c_cflag |= CS8;
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;
  ::tcsetattr(fd, TCSAFLUSH, &t);
  g_active = 1;
  tty->write(enter_seq);
  return tty;
}

#endif

}  // namespace cxxman::term
