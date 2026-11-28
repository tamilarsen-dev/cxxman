// Entry point: wires the real environment and terminal into app::run.
#include "app/app.hpp"
#include "sys/browser.hpp"
#include "sys/clipboard.hpp"
#include "sys/paths.hpp"
#include "term/terminal.hpp"
#include "term/tty.hpp"
#include "tui/finder.hpp"
#include "tui/finder_model.hpp"
#include "tui/pager.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <vector>

namespace {

using namespace cxxman;

// Shows `io.out` in the built-in pager when run() allowed it and it does not
// fit on the screen (PLAN §4.8); true when it was shown.
bool page(app::Output& io, const term::Info& t) {
  if (io.page == app::Paging::Off || io.out.empty()) return false;
  const auto lines = static_cast<std::size_t>(std::ranges::count(io.out, '\n'));
  if (io.page == app::Paging::Auto && (t.rows == 0 || lines < t.rows)) return false;
  auto tty = term::open_fullscreen();  // restores the terminal when it goes
  if (!tty) return false;
  tui::page(*tty, io.out);
  return true;
}

// The interactive finder (PLAN §4.6); false when there is no terminal to
// draw on. Ctrl-O leaves its text in io.out to print.
bool finder(const app::FinderRequest& r, const Catalog& cat, app::Output& io) {
  auto tty = term::open_fullscreen();  // restores the terminal when it goes
  if (!tty) return false;
  const tui::FinderModel model(cat);
  tui::Finder f(cat, model, {r.style, r.ascii, r.std, r.strict});
  tui::Effects fx;
  fx.copy = [&](const std::string& text) {
    if (sys::copy(text)) return tui::Effects::Copied::Clipboard;
    tty->write(sys::osc52(text));
    return tui::Effects::Copied::Terminal;
  };
  fx.open = [](const std::string& url) { return app::allowed_url(url) && sys::open_url(url); };
  io.out = tui::session(*tty, f, fx);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  term::Console console;  // restored on every exit path below

  app::Context ctx;
  ctx.env = [](const char* name) { return sys::getenv(name); };
  const auto& t = console.info();
  ctx.term = {t.out_tty, t.columns, t.vt, t.utf8, t.in_tty};
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
  bool paged = false;
  try {
    if (io.finder && !finder(*io.finder, ctx.catalog, io)) {
      io.err += "cxxman: cannot open the terminal for the finder; try 'cxxman --help'\n";
      code = 2;
    }
    paged = page(io, t);
  } catch (const std::exception& e) {  // the terminal is restored by now
    io.err += std::string("cxxman: internal error: ") + e.what() + "\n";
    code = 3;
  }
  // A reader that went away (cxxman sort | head -1) is not an error.
  if (!paged && !term::write_out(io.out)) return 0;
  term::write_err(io.err);
  return code;
}
