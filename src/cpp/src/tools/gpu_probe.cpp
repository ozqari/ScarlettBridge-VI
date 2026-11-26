// sb_gpu_probe: adapter enumeration + D3D12 device + WARP smoke test.
// Emits a JSON hardware report to stdout or a file. Works with no game files.
#include <cstdio>
#include <iostream>
#include "core/filesystem_guards.h"
#include "core/hash.h"
#include "core/logging.h"
#include "core/version.h"
#include "graphics/abstraction.h"
#include "platform/windows_system.h"

int main(int argc, char** argv) {
  sb::log::init("", sb::log::Severity::Warn, sb::log::Severity::Error);
  std::string outPath;
  bool warpOnly = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--out" && i + 1 < argc) outPath = argv[++i];
    if (a == "--warp") warpOnly = true;
  }
  std::string json = "{";
  json += "\"commit\":\"" + std::string(sb::kBuildId) + "\",\"debugLayer\":false";
  if (auto sys = sb::sysinfo::collect(); sys.ok()) {
    const auto& s = sys.value();
    json += ",\"cpu\":\"x86-64\",\"logical\":" + std::to_string(s.logicalProcessors) +
            ",\"ramMB\":" + std::to_string(s.ramMB) + ",\"os\":\"" + s.osVersion +
            "\",\"windowsBuild\":\"" + s.windowsBuild + "\"";
  }
  auto backend = sb::gfx::d3d12Backend();
  if (!backend.ok()) {
    // Capability skip (exit 77), not a failure: no D3D12 on this machine.
    // Convention adopted from Xbox2Windows CI (skips separate from pass/fail).
    std::cout << "{\"skip\":\"" << backend.error().message << "\"}\n";
    return 77;
  } else {
    auto adapters = backend.value()->enumerateAdapters(warpOnly);
    if (!adapters.ok()) {
      json += ",\"error\":\"" + adapters.error().message + "\"}";
    } else {
      int pick = -1;
      for (std::size_t i = 0; i < adapters.value().size(); ++i)
        if (!adapters.value()[i].isSoftware) {
          pick = static_cast<int>(i);
          break;
        }
      if (pick < 0) pick = 0;  // WARP fallback, labelled in output
      const auto& a = adapters.value()[static_cast<std::size_t>(pick)];
      json += ",\"adapter\":\"" + a.description + "\",\"vendor\":" +
              std::to_string(a.vendorId) + ",\"vramMB\":" + std::to_string(a.dedicatedMB) +
              ",\"software\":" + std::string(a.isSoftware ? "true" : "false");
      if (auto d = backend.value()->createDevice(pick, false, false); !d.ok()) {
        json += ",\"deviceError\":\"" + d.error().message + "\"}";
      } else {
        auto smoke = backend.value()->smokeTriangle();
        if (smoke.ok()) {
          const auto& s = smoke.value();
          json += ",\"smoke\":{\"compileMs\":" + std::to_string(s.compileMs) +
                  ",\"drawMs\":" + std::to_string(s.drawMs) + ",\"pixel\":\"" + s.pixelHash +
                  "\",\"expected\":" + (s.pixelAsExpected ? "true" : "false") +
                  ",\"ticks\":" + std::to_string(s.timestampTicks) + ",\"tickFreq\":" +
                  std::to_string(s.timestampFreq) +
                  ",\"tsResolved\":" + (s.timestampResolved ? "true" : "false") +
                  ",\"removed\":\"" + backend.value()->deviceRemovedReason() + "\"}";
        } else {
          json += ",\"smokeError\":\"" + smoke.error().message + "\"}";
        }
        backend.value()->destroyDevice();
        json += "}";
      }
    }
  }
  if (!outPath.empty()) {
    // Probe reports are machine-derived data: refuse in-tree destinations.
    if (auto g = sb::fs::ensureOutsideRepo(outPath); !g.ok()) {
      std::cerr << "{\"error\":\"" << g.error().message << "\"}\n";
      return 2;
    }
    // Minimal write (tool output; errors to stderr via log).
    if (FILE* f = std::fopen(outPath.c_str(), "w")) {
      std::fputs(json.c_str(), f);
      std::fclose(f);
    }
  } else {
    std::cout << json << "\n";
  }
  return 0;
}
