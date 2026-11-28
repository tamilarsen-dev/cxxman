// Shell completion (PLAN §12.1 M4): the scripts `--completions <shell>`
// prints, and the candidates the hidden `--complete <prefix>` lists for them.
#pragma once

#include "model/lookup.hpp"

#include <string>
#include <string_view>

namespace cxxman::app {

// The script for bash, zsh, fish or powershell; "" for another shell.
std::string_view completion_script(std::string_view shell);

// One candidate per line: options when `prefix` starts with '-', else the
// names `cxxman <name>` opens (cards and index names) that start with
// `prefix`, ignoring ASCII case; a leading "std::" is kept.
std::string complete(const Catalog& cat, std::string_view prefix);

}  // namespace cxxman::app
