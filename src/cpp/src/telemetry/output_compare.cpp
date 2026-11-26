#include "telemetry/output_compare.h"

#include <cstring>
#include <sstream>

namespace sb {
namespace diag {

CompareReport decideCompare(
    const OutputContract* contract,
    const std::array<bool, kMaxCompareOutputs>& compared,
    const std::array<CompareSnapshot, kMaxCompareOutputs>& before,
    const std::array<CompareSnapshot, kMaxCompareOutputs>& after,
    const std::array<std::size_t, kMaxCompareOutputs>& bytes) {
  CompareReport report;
  try {
    if (contract == nullptr) return report;
    const std::size_t declared =
        contract->second == kNoOutputArg ? std::size_t{1} : std::size_t{2};
    std::size_t checked = 0;
    for (std::size_t i = 0; i < declared; ++i) {
      if (!compared[i] || bytes[i] == 0) continue;
      ++checked;
      // Caller-sized buffers compare within the snapshot bound (the
      // upstream caller promises the bound; we enforce it).
      const std::size_t n =
          bytes[i] > kMaxSnapshotBytes ? kMaxSnapshotBytes : bytes[i];
      if (std::memcmp(before[i].data(), after[i].data(), n) != 0)
        report.changed[report.changedCount++] = i;
    }
    report.partial = checked != declared;
    if (checked == 0) {
      report.outcome = CompareOutcome::Unreadable;
    } else if (report.changedCount != 0) {
      report.outcome = CompareOutcome::Changed;
    } else {
      report.outcome = CompareOutcome::Same;
    }
  } catch (...) {
    // Never throws: on allocation failure the report stays Unknown.
    report = CompareReport{};
  }
  return report;
}

std::string formatCompare(const CompareReport& report, const OutputContract* contract) {
  try {
    switch (report.outcome) {
      case CompareOutcome::Unknown:
        return "?";
      case CompareOutcome::Unreadable:
        return "unreadable";
      case CompareOutcome::Same: {
        std::string s = "same";
        if (report.partial) s += ",partial";
        return s;
      }
      case CompareOutcome::Changed: {
        const int first = contract != nullptr ? contract->first : 0;
        const int second = (contract != nullptr && contract->second != kNoOutputArg)
                               ? contract->second
                               : -1;
        const int a0 = report.changed[0] == 0 ? first : second;
        std::ostringstream o;
        o << "changed:" << a0;
        if (report.changedCount > 1) {
          const int a1 = report.changed[1] == 0 ? first : second;
          o << "," << a1;
        }
        if (report.partial) o << ",partial";
        return o.str();
      }
    }
  } catch (...) {
  }
  return "?";
}

}  // namespace diag
}  // namespace sb
