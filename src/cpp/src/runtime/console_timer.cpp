#include "runtime/console_timer.h"

#include <cstring>

namespace sb {

unsigned clampTimerRequestMs(unsigned ms) {
  if (ms < kConsoleTimerResolutionMs) return kConsoleTimerResolutionMs;
  if (ms > kDesktopTimerResolutionMs) return kDesktopTimerResolutionMs;
  return ms;
}

bool efficiencyOptOut(const char* envValue) {
  if (envValue == nullptr) return true;
  return std::strcmp(envValue, "1") != 0;
}

}  // namespace sb
