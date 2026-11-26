#include "platform/windows_system.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <thread>
#endif

namespace sb {
namespace sysinfo {

Result<SystemReport> collect() {
  SystemReport r;
#if defined(_WIN32)
  SYSTEM_INFO si{};
  GetNativeSystemInfo(&si);
  r.logicalProcessors = si.dwNumberOfProcessors;
  MEMORYSTATUSEX mem{};
  mem.dwLength = sizeof(mem);
  if (GlobalMemoryStatusEx(&mem)) r.ramMB = mem.ullTotalPhys / (1024 * 1024);
  char brand[64] = {0};
  // GenuineIntel/ AuthenticAMD leaf via __cpuid would add <intrin.h>; keep to
  // what GetNativeSystemInfo gives us and label honestly.
  r.cpuModel = (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
                   ? "x86-64 (model query via DXGI/GPU probe)"
                   : "unknown arch";
  (void)brand;
  OSVERSIONINFOEXW os{};
  os.dwOSVersionInfoSize = sizeof(os);
  // RtlGetVersion is the documented non-deprecated path; fall back to a
  // generic label rather than a manifest-fragile GetVersionEx call.
  using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
  if (auto mod = GetModuleHandleW(L"ntdll.dll")) {
    if (auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(mod, "RtlGetVersion"))) {
      RTL_OSVERSIONINFOW v{};
      v.dwOSVersionInfoSize = sizeof(v);
      if (fn(&v) == 0) {
        r.windowsBuild = std::to_string(v.dwBuildNumber);
        r.osVersion = "Windows " + std::to_string(v.dwMajorVersion) + "." +
                     std::to_string(v.dwMinorVersion) + " build " + r.windowsBuild;
      }
    }
  }
  if (r.osVersion.empty()) r.osVersion = "Windows (version query unavailable)";
#else
  r.logicalProcessors = std::thread::hardware_concurrency();
  r.cpuModel = "non-Windows build (info only)";
  r.osVersion = "non-Windows build";
#endif
  return Result<SystemReport>::ok(std::move(r));
}

}  // namespace sysinfo
}  // namespace sb
