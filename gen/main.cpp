// cxxman-gen: lint data/ and generate data.cpp and the snippet tests.
//
//   cxxman-gen [--check] [--emit DIR] [--snippets DIR]
//              [--changelog FILE --version X.Y.Z] DATA_DIR
//
// The linter always runs; --check also prints its warnings. Nothing is
// written when there are errors. Exit codes: 0 ok, 1 lint errors, 2 usage.
#include "schema.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>

namespace fs = std::filesystem;
using namespace cxxman::gen;

namespace {

int usage(std::string_view problem) {
  std::cerr << "cxxman-gen: " << problem << "\n"
            << "usage: cxxman-gen [--check] [--emit DIR] [--snippets DIR]"
               " [--changelog FILE --version X.Y.Z] DATA_DIR\n";
  return 2;
}

std::string today() {
  using namespace std::chrono;
  const year_month_day ymd{floor<days>(system_clock::now())};
  char buf[16];
  std::snprintf(buf, sizeof buf, "%04d-%02u-%02u", static_cast<int>(ymd.year()),
                static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()));
  return buf;
}

std::string read(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

bool write(const fs::path& p, std::string_view content) {
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
  return static_cast<bool>(out);
}

// Keeps timestamps of unchanged files so the snippet build stays incremental.
bool write_if_changed(const fs::path& p, std::string_view content) {
  std::error_code ec;
  if (fs::exists(p, ec) && read(p) == content) return true;
  return write(p, content);
}

}  // namespace

int main(int argc, char** argv) {
  bool check = false;
  fs::path emit_dir, snippets_dir, changelog_file, data_dir;
  std::string version;
  for (int i = 1; i < argc; ++i) {
    std::string_view a = argv[i];
    auto value = [&](fs::path& out) {
      if (i + 1 >= argc) return false;
      out = argv[++i];
      return true;
    };
    if (a == "--check") {
      check = true;
    } else if (a == "--emit") {
      if (!value(emit_dir)) return usage("--emit needs a directory");
    } else if (a == "--snippets") {
      if (!value(snippets_dir)) return usage("--snippets needs a directory");
    } else if (a == "--changelog") {
      if (!value(changelog_file)) return usage("--changelog needs a file");
    } else if (a == "--version") {
      if (i + 1 >= argc) return usage("--version needs a value");
      version = argv[++i];
    } else if (a.starts_with("-")) {
      return usage("unknown option '" + std::string(a) + "'");
    } else if (data_dir.empty()) {
      data_dir = a;
    } else {
      return usage("more than one data directory");
    }
  }
  if (data_dir.empty()) return usage("no data directory");
  if (changelog_file.empty() != version.empty())
    return usage("--changelog and --version go together");

  Diags d;
  Data data = load(data_dir, d);
  if (d.errors() == 0) lint(data, today(), d);

  std::string changelog;
  if (!changelog_file.empty()) {
    std::string md = read(changelog_file);
    std::erase(md, '\r');  // CRLF checkouts
    auto section = changelog_section(md, version);
    if (!section)
      d.error({changelog_file.generic_string(), 0}, "no '## " + version + "' section");
    else
      changelog = *section;
  }

  int warnings = 0;
  for (const auto& x : d.all()) {
    if (!x.error) {
      ++warnings;
      if (!check) continue;
    }
    std::cerr << x.loc.file;
    if (x.loc.line > 0) std::cerr << ':' << x.loc.line;
    std::cerr << (x.error ? ": error: " : ": warning: ") << x.message << '\n';
  }
  if (d.errors() > 0) {
    std::cerr << "cxxman-gen: " << d.errors() << " error(s), " << warnings << " warning(s)\n";
    return 1;
  }

  if (!emit_dir.empty()) {
    fs::create_directories(emit_dir);
    // Always rewritten: the build reruns this step only when data/ changed.
    if (!write(emit_dir / "data.cpp", emit_data_cpp(data, changelog))) {
      std::cerr << "cxxman-gen: cannot write " << (emit_dir / "data.cpp").string() << '\n';
      return 1;
    }
  }

  std::size_t snippet_count = 0;
  if (!snippets_dir.empty()) {
    fs::create_directories(snippets_dir);
    auto snippets = make_snippets(data);
    snippet_count = snippets.size();
    std::set<fs::path> keep;
    for (const auto& s : snippets) {
      auto p = snippets_dir / (s.name + ".cpp");
      keep.insert(p);
      if (!write_if_changed(p, s.source)) {
        std::cerr << "cxxman-gen: cannot write " << p.string() << '\n';
        return 1;
      }
    }
    if (!write_if_changed(snippets_dir / "manifest.cmake", emit_manifest(snippets))) {
      std::cerr << "cxxman-gen: cannot write the snippet manifest\n";
      return 1;
    }
    for (const auto& f : fs::directory_iterator(snippets_dir))
      if (f.path().extension() == ".cpp" && !keep.contains(f.path())) fs::remove(f.path());
  }

  std::cout << "cxxman-gen: " << data.entries.size() << " cards, " << data.recipes.size()
            << " recipes, " << data.compares.size() << " compares, " << data.headers.size()
            << " header files, " << data.index.size() << " index names";
  if (!snippets_dir.empty()) std::cout << ", " << snippet_count << " snippets";
  if (warnings > 0) std::cout << ", " << warnings << " warning(s)";
  std::cout << '\n';
  return 0;
}
