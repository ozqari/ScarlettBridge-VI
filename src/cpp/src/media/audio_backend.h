#pragma once
// Audio backend abstraction. Enumeration is implemented (Windows); streaming
// is an explicit Unsupported until the Windows backend lands with tests.
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {
namespace audio {

struct DeviceInfo {
  std::string id;
  std::string name;
  bool isDefault = false;
};

class IAudioBackend {
 public:
  virtual ~IAudioBackend() = default;
  virtual Result<std::vector<DeviceInfo>> enumerate() = 0;
  virtual Result<void> playTestTone(int deviceIndex) = 0;  // 440 Hz, 0.5 s
};

Result<IAudioBackend*> windowsBackend();

}  // namespace audio
}  // namespace sb
