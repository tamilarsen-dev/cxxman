// A single-pass C++ lexer for coloring snippets (PLAN §8.4). No regex; any
// input is accepted, and what it does not recognize stays Role::Plain.
#pragma once

#include "render/layout.hpp"

#include <string_view>
#include <vector>

namespace cxxman::render {

// One role per byte of `code`.
std::vector<Role> highlight(std::string_view code);

}  // namespace cxxman::render
