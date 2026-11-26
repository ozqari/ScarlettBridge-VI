#include "telemetry/driver_watch.h"

namespace sb {
namespace diag {

bool DriverWatch::noteError() {
  try {
    std::lock_guard<std::mutex> lock(mu_);
    ++errors_;
    if (!tripped_) {
      tripped_ = true;
      return true;
    }
    return false;
  } catch (...) {
    return false;
  }
}

bool DriverWatch::tripped() const {
  try {
    std::lock_guard<std::mutex> lock(mu_);
    return tripped_;
  } catch (...) {
    return false;
  }
}

std::uint64_t DriverWatch::errors() const {
  try {
    std::lock_guard<std::mutex> lock(mu_);
    return errors_;
  } catch (...) {
    return 0;
  }
}

void DriverWatch::reset() {
  try {
    std::lock_guard<std::mutex> lock(mu_);
    errors_ = 0;
    tripped_ = false;
  } catch (...) {
  }
}

}  // namespace diag
}  // namespace sb
