// Opening a page in the default browser (PLAN §2: --web).
#pragma once

#include <string_view>

namespace cxxman::sys {

// ShellExecuteW on Windows, `open` on macOS, `xdg-open` elsewhere; on WSL
// also `wslview`, then `explorer.exe`. The caller checks the URL first
// (app::allowed_url). Returns false when nothing could open it.
bool open_url(std::string_view url);

}  // namespace cxxman::sys
