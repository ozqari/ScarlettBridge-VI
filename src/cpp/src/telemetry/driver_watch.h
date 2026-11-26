#pragma once
// Driver-error trip latch for the diagnostics path.
//
// Behavior informed by victory-fork `crates/victory-gpu/src/drvwatch.rs`
// (`seen()`: every driver error is counted and the first one trips a
// switch that stays on for the rest of the run; `tripped()` / `errors()`;
// unit test: first error trips, every error is counted). Independently
// implemented here in portable C++ with our own types and wording. The
// OS collection (Windows EventLog subscription to nvlddmkm/amdkmdag/
// amdwddmg/igfx*/Display, VICTORY_DRIVER_WATCH=0 opt-out, Auto HDR 4125
// lesson) is NOT implemented — this tree performs no OS event-log
// collection — only the portable counting latch travels. Unit-tested in
// sb_tests.
#include <cstdint>
#include <mutex>

namespace sb {
namespace diag {

// Counts driver errors and latches on the first one. Plain object, no
// threads of its own, no OS calls: an EventLog (or any other) source
// calls noteError() once per entry. Never throws.
class DriverWatch {
 public:
  DriverWatch() = default;

  // Records one driver error: the count grows by one and the first call
  // trips the latch for the rest of the run. Returns true when this call
  // was the trip (the latch was clear before). Never throws.
  bool noteError();

  // Whether any driver error has been recorded in this run. Never throws.
  bool tripped() const;
  // Driver errors recorded since construction (or the last reset).
  // Never throws.
  std::uint64_t errors() const;
  void reset();

 private:
  mutable std::mutex mu_;
  std::uint64_t errors_ = 0;
  bool tripped_ = false;
};

}  // namespace diag
}  // namespace sb
