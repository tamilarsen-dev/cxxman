#include "sys/paths.hpp"

#include <cstdlib>

namespace cxxman::sys {

const char* getenv(const char* name) {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)  // the pointer is only read, right away
#endif
  return std::getenv(name);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

std::filesystem::path config_file(const std::function<const char*(const char*)>& env) {
  auto get = [&](const char* name) -> std::filesystem::path {
    const char* v = env(name);
    return v && *v ? std::filesystem::path(v) : std::filesystem::path{};
  };
#ifdef _WIN32
  const auto base = get("APPDATA");
#else
  auto base = get("XDG_CONFIG_HOME");
  if (base.empty()) {
    const auto home = get("HOME");
    if (!home.empty()) base = home / ".config";
  }
#endif
  return base.empty() ? std::filesystem::path{} : base / "cxxman" / "config.toml";
}

}  // namespace cxxman::sys
