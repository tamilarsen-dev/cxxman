// Environment and well-known paths.
#pragma once

#include <filesystem>
#include <functional>

namespace cxxman::sys {

// std::getenv without MSVC's deprecation warning.
const char* getenv(const char* name);

// %APPDATA%\cxxman\config.toml on Windows; $XDG_CONFIG_HOME/cxxman/config.toml
// (default ~/.config) elsewhere, macOS included (PLAN §7.5). Empty when the
// base directory is unknown.
std::filesystem::path config_file(const std::function<const char*(const char*)>& env);

}  // namespace cxxman::sys
