#pragma once
// Formal runtime lifecycle: Created -> Validating -> InitializingPlatform ->
// InitializingGraphics -> LoadingModules -> Ready -> Running -> Stopping ->
// Stopped, any state -> Failed. Illegal transitions are rejected errors.
// A restart always creates a fresh instance (new Lifecycle object).
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {

enum class RuntimeState {
  Created,
  Validating,
  InitializingPlatform,
  InitializingGraphics,
  LoadingModules,
  Ready,
  Running,
  Stopping,
  Stopped,
  Failed
};

std::string toString(RuntimeState s);
bool isLegalTransition(RuntimeState from, RuntimeState to);

class Lifecycle {
 public:
  Lifecycle() = default;
  RuntimeState state() const { return state_; }
  const std::string& reason() const { return reason_; }
  Result<void> transition(RuntimeState to, const std::string& reason = "");
  const std::vector<std::pair<RuntimeState, std::string>>& history() const { return history_; }

 private:
  RuntimeState state_ = RuntimeState::Created;
  std::string reason_;
  std::vector<std::pair<RuntimeState, std::string>> history_;
};

}  // namespace sb
