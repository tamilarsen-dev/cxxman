// Running another program without a shell (PLAN §7.2).
#pragma once

#include <span>
#include <string>
#include <string_view>

namespace cxxman::sys {

// Runs argv[0] (looked up in PATH) with `input` on its stdin and its
// stdout/stderr sent to the null device, and waits for it. Returns its exit
// code, or -1 when it could not be started. POSIX only for now: on Windows it
// returns -1 (M7 adds CreateProcessW for the updater).
int run(std::span<const std::string> argv, std::string_view input = {});

}  // namespace cxxman::sys
