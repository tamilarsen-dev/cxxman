// Entry point: wires the real environment and terminal into app::run.
#include "app/app.hpp"
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
  // A reader that went away (cxxman sort | head -1) is not an error.
  if (!term::write_out(io.out)) return 0;
  term::write_err(io.err);
  return code;
}
