#include "runtime/lifecycle.h"

namespace sb {

std::string toString(RuntimeState s) {
  switch (s) {
    case RuntimeState::Created: return "Created";
    case RuntimeState::Validating: return "Validating";
    case RuntimeState::InitializingPlatform: return "InitializingPlatform";
    case RuntimeState::InitializingGraphics: return "InitializingGraphics";
    case RuntimeState::LoadingModules: return "LoadingModules";
    case RuntimeState::Ready: return "Ready";
    case RuntimeState::Running: return "Running";
    case RuntimeState::Stopping: return "Stopping";
    case RuntimeState::Stopped: return "Stopped";
    case RuntimeState::Failed: return "Failed";
  }
  return "?";
}

bool isLegalTransition(RuntimeState from, RuntimeState to) {
  if (to == RuntimeState::Failed) return from != RuntimeState::Failed;
  switch (from) {
    case RuntimeState::Created: return to == RuntimeState::Validating;
    case RuntimeState::Validating: return to == RuntimeState::InitializingPlatform;
    case RuntimeState::InitializingPlatform: return to == RuntimeState::InitializingGraphics;
    case RuntimeState::InitializingGraphics: return to == RuntimeState::LoadingModules;
    case RuntimeState::LoadingModules: return to == RuntimeState::Ready;
    case RuntimeState::Ready: return to == RuntimeState::Running || to == RuntimeState::Stopping;
    case RuntimeState::Running: return to == RuntimeState::Stopping;
    case RuntimeState::Stopping: return to == RuntimeState::Stopped;
    case RuntimeState::Stopped: return to == RuntimeState::Validating;
    case RuntimeState::Failed: return to == RuntimeState::Validating;
  }
  return false;
}

Result<void> Lifecycle::transition(RuntimeState to, const std::string& reason) {
  if (!isLegalTransition(state_, to))
    return Result<void>::fail(Error::make(
        "runtime", 1, "illegal transition " + toString(state_) + " -> " + toString(to)));
  state_ = to;
  reason_ = reason;
  history_.emplace_back(to, reason);
  return Result<void>::success();
}

}  // namespace sb
