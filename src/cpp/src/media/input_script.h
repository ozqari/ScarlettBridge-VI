#pragma once
// Deterministic scripted gamepad input for DEMO mode and tests: parse a
// comma-separated button/axis script and play back the pad state at any
// second. No OS calls, no devices, no game content.
//
// Behavior informed by Xbox2Windows `src/startup_stubs/input_script.h` and
// its `tests/startup_gameinput_script*.cpp` contracts at `8c9f168f4746`
// (mask@seconds holds with 0.3 s default, axis ranges, relative @+times,
// base=poll/start, 256-entry cap, 1-based rejection index + reason, button
// OR with later-axis-wins playback). Independently implemented here over our
// own state type; the presenter-coupled parts (waits, marks, groups and the
// picture-signal timeline) are NOT implemented — such items are kept as
// rejections instead of running. Unit-tested in sb_tests.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sb {
namespace input {

// At most this many timed entries are kept; the rest are rejected.
inline constexpr std::size_t kMaxScriptEntries = 256;
// Hold length when `@<s>` names no `+<d>`.
inline constexpr double kDefaultHoldSeconds = 0.3;

// Axis order: the two triggers, then the two sticks.
enum class ScriptAxis : int {
  LeftTrigger = 0,
  RightTrigger = 1,
  LeftX = 2,
  LeftY = 3,
  RightX = 4,
  RightY = 5,
  Count = 6,
};

struct ScriptEntry {
  int axis = -1;  // -1 = button mask, else (int)ScriptAxis
  std::uint32_t mask = 0;
  float value = 0.0f;
  double at = 0.0;
  double duration = kDefaultHoldSeconds;
  std::size_t item = 0;  // 1-based position among the non-empty items
};

struct ScriptRejection {
  std::size_t index = 0;  // 1-based, like the entries' item numbers
  std::string reason;     // exact contract wording (see parseInputScript)
  std::string text;       // the trimmed item, as written
};

struct InputScript {
  bool fromProcessStart = false;  // base=start seen (else base=poll)
  std::size_t items = 0;          // non-empty items, accepted or not
  std::vector<ScriptEntry> entries;
  std::vector<ScriptRejection> rejections;
};

struct ScriptState {
  std::uint32_t buttons = 0;
  std::array<float, 6> axes{};
  std::array<bool, 6> active{};
};

// Parse `text` (comma-separated; empty items skipped, never counted).
// Accepted item shapes: `<hex>@<s>[+<d>]`, `<axis>=<v>@<s>[+<d>]`
// (`lt rt` in [0,1], `lx ly rx ry` in [-1,1]), `base=poll|start` as the
// very first item. Rejection reasons, exactly: "missing @<seconds>",
// "bad time", "bad duration", "trailing text", "bad mask",
// "value out of range [-1, 1]", "value out of range [0, 1]",
// "unknown token", "bad value", "base= must be the first item",
// "unknown base (use poll or start)", "over 256 entries", plus
// "unsupported directive (waits, marks and groups need a presenter)" for
// the presenter-coupled items this subset leaves out. Never throws.
InputScript parseInputScript(const std::string& text);

// Pad state at second `t`: active entries OR their button masks; on one
// axis the later item wins. Never throws.
ScriptState playInputScript(const InputScript& script, double t);

// Earliest entry start / latest entry end (0 when the script is empty).
// Never throws.
double firstInputStart(const InputScript& script);
double lastInputEnd(const InputScript& script);

}  // namespace input
}  // namespace sb
