#include "telemetry/fault_filter.h"

namespace sb {
namespace diag {

bool FaultBudget::isFault(std::uint32_t code) {
  if (code & 0x20000000u) return false;  // customer code (C++/CLR exceptions)
  if (code == 0x80000003u) return true;  // breakpoint / int3
  return (code & 0xC0000000u) == 0xC0000000u;
}

FaultDecision FaultBudget::evaluate(std::uint32_t code, bool isMapped,
                                    std::uint64_t faultAddr) {
  FaultDecision d;
  if (!isFault(code) || !isMapped) return d;
  d.number = ++reported_;
  d.newSite = sites_.size() < maxSites_ && sites_.insert(faultAddr).second;
  if (d.number > max_ && !d.newSite) return d;  // quota spent, known site
  d.shouldReport = true;
  d.brief = d.number > full_;
  return d;
}

bool FaultBudget::tryEnter() {
  if (inside_) return false;
  inside_ = true;
  return true;
}

void FaultBudget::exit() { inside_ = false; }

void FaultBudget::reset() {
  reported_ = 0;
  sites_.clear();
  inside_ = false;
}

}  // namespace diag
}  // namespace sb
