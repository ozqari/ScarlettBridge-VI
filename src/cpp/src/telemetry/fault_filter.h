#pragma once
// Fault-report budgeting: which hardware faults get a full report, a one-line
// note, or nothing.
//
// Idea informed by five-star-recomp FaultReporter.cs (vectored handler with
// ContinueSearch/ContinueExecution, NTSTATUS severity + customer-bit test,
// mapped-frame search, full-3 / max-64 / sites-1024 quotas, thread reentrancy
// guard, new-site-despite-quota). Independently implemented here as a small
// portable policy object; no OS handler code, no execution.
#include <cstdint>
#include <set>

namespace sb {
namespace diag {

struct FaultDecision {
  bool shouldReport = false;
  bool brief = false;  // true: one line; false: full register/stack/call report
  unsigned number = 0;
  bool newSite = false;
};

class FaultBudget {
 public:
  explicit FaultBudget(unsigned full = 3, unsigned max = 64, unsigned sites = 1024)
      : full_(full), max_(max), maxSites_(sites) {}
  // NTSTATUS test: customer codes (e.g. C++ exceptions) are never hardware
  // faults; breakpoints and error-severity codes are.
  static bool isFault(std::uint32_t code);
  // isMapped: a mapped image was found in the fault's stack vicinity.
  FaultDecision evaluate(std::uint32_t code, bool isMapped, std::uint64_t faultAddr);
  // Reentrancy guard: only one report is formatted on a thread at a time.
  bool tryEnter();
  void exit();
  unsigned reported() const { return reported_; }
  void reset();

 private:
  unsigned full_, max_, maxSites_;
  unsigned reported_ = 0;
  std::set<std::uint64_t> sites_;
  bool inside_ = false;
};

}  // namespace diag
}  // namespace sb
