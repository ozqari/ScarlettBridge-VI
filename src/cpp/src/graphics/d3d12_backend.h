#pragma once
// D3D12 smoke-test backend (Windows, public SDK headers only). Implements
// adapter enumeration, debug-layer device creation, and an offscreen
// triangle with pixel readback. Swapchain/present stay Unsupported until the
// presentation path has synthetic tests.
#include "graphics/abstraction.h"

namespace sb {
namespace gfx {

class D3D12Backend : public IGraphicsBackend {
 public:
  ~D3D12Backend() override { destroyDevice(); }
  Result<std::vector<AdapterInfo>> enumerateAdapters(bool includeSoftware) override;
  Result<void> createDevice(int adapterIndex, bool debugLayer, bool dred) override;
  Result<SmokeReport> smokeTriangle() override;
  Result<void> createSwapchain(int width, int height, bool vsync) override;
  Result<void> present() override;
  void destroyDevice() override;
  std::string deviceRemovedReason() override;

 private:
  bool deviceReady() const { return deviceReady_; }
  bool deviceReady_ = false;
};

}  // namespace gfx
}  // namespace sb
