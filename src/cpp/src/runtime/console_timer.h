#pragma once
// Console timer policy for the host process.
//
// Idea informed by victory-fork crates/victory-hle/src/runtime.rs
// use_console_timers (titles pace themselves with ~4 ms waits; a desktop
// process idles in 15.6 ms ticks unless it asks for finer ones, and the
// request is dropped while hidden unless power-throttling is opted out).
// Independently implemented here as a portable policy object; the actual
// timeBeginPeriod/SetProcessInformation calls stay in the Windows backend.
// No code copied.
namespace sb {

inline constexpr unsigned kConsoleTimerResolutionMs = 1;
inline constexpr unsigned kDesktopTimerResolutionMs = 16;  // ~15.6 ms ticks

// Requested resolution, clamped to the sane [1, 16] ms range.
unsigned clampTimerRequestMs(unsigned ms);
// True unless the environment explicitly leaves the system its choice with
// VICTORY_EFFICIENCY_MODE=1 (value compared exactly, like upstream).
bool efficiencyOptOut(const char* envValue);

struct TimerPolicy {
  unsigned resolutionMs = kConsoleTimerResolutionMs;
  bool keepEfficiencyOff = true;  // opt out of efficiency mode / timer ignore
};

inline TimerPolicy consoleTimerPolicy(const char* efficiencyEnv) {
  TimerPolicy p;
  p.resolutionMs = kConsoleTimerResolutionMs;
  p.keepEfficiencyOff = efficiencyOptOut(efficiencyEnv);
  return p;
}

}  // namespace sb
