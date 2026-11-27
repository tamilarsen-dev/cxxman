// The real terminal: what it can do, and writing to it (PLAN §4.10).
#pragma once

#include <cstddef>
#include <string_view>

namespace cxxman::term {

struct Info {
  bool out_tty = false, err_tty = false, in_tty = false;
  std::size_t columns = 0;  // 0: unknown
  bool vt = false;          // escape codes are understood
  bool utf8 = true;         // the console shows UTF-8
};

// Prepares the console (VT processing and UTF-8 output on Windows; SIGPIPE
// ignored on POSIX) and restores it when destroyed.
class Console {
 public:
  Console();
  ~Console();
  Console(const Console&) = delete;
  Console& operator=(const Console&) = delete;

  const Info& info() const noexcept { return info_; }

 private:
  Info info_;
  unsigned long old_mode_ = 0;
  unsigned old_cp_ = 0;
  bool restore_mode_ = false;
};

// Write everything; false when the reader went away (broken pipe).
bool write_out(std::string_view s);
bool write_err(std::string_view s);

}  // namespace cxxman::term
