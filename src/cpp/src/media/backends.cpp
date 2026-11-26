#include "media/audio_backend.h"
#include "media/input_backend.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xinput.h>
#endif

namespace sb {
namespace audio {
namespace {
class WindowsAudio : public IAudioBackend {
 public:
  Result<std::vector<DeviceInfo>> enumerate() override {
#if defined(_WIN32)
    // Full WASAPI enumeration is Phase 10; report the system default only,
    // honestly labelled, so the UI never shows a fake device list.
    DeviceInfo d;
    d.id = "default";
    d.name = "System default output (WASAPI enumeration pending)";
    d.isDefault = true;
    return Result<std::vector<DeviceInfo>>::ok(std::vector<DeviceInfo>{d});
#else
    return Result<std::vector<DeviceInfo>>::fail(
        Error::make("audio", 1, "audio enumeration is Windows-only"));
#endif
  }
  Result<void> playTestTone(int) override {
    return Result<void>::fail(
        Error::make("audio", 2, "Unsupported: WASAPI streaming lands in Phase 10 with tests"));
  }
};
}  // namespace
Result<IAudioBackend*> windowsBackend() {
  static WindowsAudio instance;
  return Result<IAudioBackend*>::ok(&instance);
}
}  // namespace audio

namespace input {
namespace {
class WindowsInput : public IInputBackend {
 public:
  Result<std::vector<GamepadInfo>> enumerateGamepads() override {
#if defined(_WIN32)
    std::vector<GamepadInfo> out;
    for (DWORD i = 0; i < 4; ++i) {
      XINPUT_STATE st{};
      if (XInputGetState(i, &st) == ERROR_SUCCESS) {
        GamepadInfo g;
        g.index = static_cast<int>(i);
        g.id = "XInput pad " + std::to_string(i);
        g.connected = true;
        out.push_back(g);
      }
    }
    return Result<std::vector<GamepadInfo>>::ok(std::move(out));
#else
    return Result<std::vector<GamepadInfo>>::fail(
        Error::make("input", 1, "gamepad enumeration is Windows-only"));
#endif
  }
  Result<InputEvent> pollOnce() override {
    return Result<InputEvent>::fail(
        Error::make("input", 2, "Unsupported: event translation lands in Phase 10 with tests"));
  }
};
}  // namespace
Result<IInputBackend*> windowsBackend() {
  static WindowsInput instance;
  return Result<IInputBackend*>::ok(&instance);
}
}  // namespace input
}  // namespace sb
