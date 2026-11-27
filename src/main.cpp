// Entry point: wires the real environment and terminal into app::run.
#include "app/app.hpp"
#include "sys/browser.hpp"
#include "sys/clipboard.hpp"
#include "sys/paths.hpp"
#include "term/terminal.hpp"

#include <exception>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  using namespace cxxman;
  term::Console console;  // restored on every exit path below

  app::Context ctx;
  ctx.env = [](const char* name) { return sys::getenv(name); };
  const auto& t = console.info();
  ctx.term = {t.out_tty, t.columns, t.vt, t.utf8};
  ctx.catalog = Catalog::builtin();
  ctx.config_file = sys::config_file(ctx.env);

  const std::vector<std::string> args(argv + 1, argv + argc);
  app::Output io;
  int code = 0;
  try {
    code = app::run(args, ctx, io);
  } catch (const std::exception& e) {
    io.err += std::string("cxxman: internal error: ") + e.what() + "\n";
    code = 3;
  }
  // What run() asked for but does not do itself (PLAN §4.9, --web).
  if (!io.open_url.empty() && !sys::open_url(io.open_url)) {
    io.err += "cxxman: could not open a browser; the page is " + io.open_url + "\n";
    code = 1;
  }
  if (!io.clipboard.empty()) {
    if (sys::copy(io.clipboard)) {
      io.err += "copied " + io.copied + "\n";
    } else if (t.out_tty) {
      // No clipboard tool: ask the terminal (works over SSH in most).
      io.out += sys::osc52(io.clipboard);
      io.err += "sent " + io.copied + " to the terminal's clipboard (OSC 52)\n";
    } else {
      io.err += "cxxman: no clipboard tool found (wl-copy, xclip or xsel)\n";
      code = 1;
    }
  }
  // A reader that went away (cxxman sort | head -1) is not an error.
  if (!term::write_out(io.out)) return 0;
  term::write_err(io.err);
  return code;
}
