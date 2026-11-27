#include "sys/browser.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <string>
#else
#include "sys/paths.hpp"
#include "sys/process.hpp"

#include <string>
#endif

namespace cxxman::sys {

#ifdef _WIN32

bool open_url(std::string_view url) {
  const int n = MultiByteToWideChar(CP_UTF8, 0, url.data(), static_cast<int>(url.size()), nullptr, 0);
  if (n <= 0) return false;
  std::wstring wide(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, url.data(), static_cast<int>(url.size()), wide.data(), n);
  // ShellExecute may use COM for some handlers (its documentation asks for this).
  const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  const auto result = ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  if (SUCCEEDED(com)) CoUninitialize();
  return reinterpret_cast<INT_PTR>(result) > 32;
}

#else

bool open_url(std::string_view url) {
  const std::string u(url);
#ifdef __APPLE__
  const std::string open[] = {"open", u};
  return run(open) == 0;
#else
  const std::string xdg[] = {"xdg-open", u};
  if (run(xdg) == 0) return true;
  const char* wsl = getenv("WSL_DISTRO_NAME");
  if (!wsl || !*wsl) return false;
  const std::string wslview[] = {"wslview", u};
  if (run(wslview) == 0) return true;
  // explorer.exe opens a URL in the Windows default browser, but its exit
  // code is 1 even then; only a failure to start it counts.
  const std::string explorer[] = {"explorer.exe", u};
  return run(explorer) >= 0;
#endif
}

#endif

}  // namespace cxxman::sys
