#pragma once
// Backend-independent graphics interfaces. Only the synthetic smoke path is
// implemented (D3D12); every other operation reports Unsupported with a
// reason instead of faking success.
#include <cstdint>
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {
namespace gfx {

struct AdapterInfo {
  std::string description;
  std::uint32_t vendorId = 0;
  std::uint32_t deviceId = 0;
  std::uint64_t dedicatedMB = 0;
  std::string umdVersion;
  bool isSoftware = false;
  bool supportsD3D12 = false;
};

struct SmokeReport {
  bool deviceCreated = false;
  double compileMs = 0.0;
  double drawMs = 0.0;
  std::uint8_t centerPixel[4] = {0, 0, 0, 0};
  std::string pixelHash;  // FNV-1a of the 4 bytes
  bool pixelAsExpected = false;
  bool timestampResolved = false;
  std::uint64_t timestampTicks = 0;  // GPU ticks between submit markers
  std::uint64_t timestampFreq = 0;   // ticks per second (GetTimestampFrequency)
  std::vector<std::string> warnings;
};

class IGraphicsBackend {
 public:
  virtual ~IGraphicsBackend() = default;
  virtual Result<std::vector<AdapterInfo>> enumerateAdapters(bool includeSoftware) = 0;
  virtual Result<void> createDevice(int adapterIndex, bool debugLayer, bool dred) = 0;
  // Offscreen synthetic triangle + readback. Must work on WARP.
  virtual Result<SmokeReport> smokeTriangle() = 0;
  virtual Result<void> createSwapchain(int width, int height, bool vsync) = 0;
  virtual Result<void> present() = 0;
  virtual void destroyDevice() = 0;
  // "S_OK" normally; HRESULT hex after a device loss. Default for backends
  // without removal reporting.
  virtual std::string deviceRemovedReason() { return "unsupported"; }
};

Result<IGraphicsBackend*> d3d12Backend();  // Windows only; Unsupported elsewhere

}  // namespace gfx
}  // namespace sb
