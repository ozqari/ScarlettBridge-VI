#pragma once
// Input abstraction: keyboard/mouse/gamepad as one event model. Enumeration
// is real (Windows Raw Input / XInput presence check); mapping lives in JSON
// and is shown in the GUI. Never installs drivers or changes system settings.
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {
namespace input {

struct GamepadInfo {
  int index = -1;
  std::string id;
  bool connected = false;
};

struct InputEvent {
  std::string device;  // "keyboard" | "mouse" | "gamepad<N>"
  std::string code;    // e.g. "KeyW", "Btn0", "Axis0+"
  double value = 0.0;
};

class IInputBackend {
 public:
  virtual ~IInputBackend() = default;
  virtual Result<std::vector<GamepadInfo>> enumerateGamepads() = 0;
  virtual Result<InputEvent> pollOnce() = 0;  // empty code = no event
};

Result<IInputBackend*> windowsBackend();

}  // namespace input
}  // namespace sb
