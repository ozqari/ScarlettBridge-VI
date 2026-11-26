#pragma once
// Declared-output comparison for opt-in traces.
//
// Behavior informed by Xbox2Windows `src/startup_stubs/api_trace.h`
// (decide(): Unknown when no contract names the outputs, Unreadable when
// nothing could be compared, Same when every compared output matched,
// Changed otherwise, plus a partial flag when some declared output went
// uncompared; format(): "?", "unreadable", "same", "changed:<args>" with
// ",partial"). Independently implemented here in portable C++ with our own
// types and wording; the provider/slot table and any tracing hooks are NOT
// implemented — no providers exist in this tree — only the pure
// compare/format core travels. The discipline matches the project's
// negative-control rule: only declared outputs are ever read, and "same"
// never claims a write was skipped. Unit-tested in sb_tests.
#include <array>
#include <cstddef>
#include <string>

namespace sb {
namespace diag {

// Argument slot meaning "no output here".
inline constexpr unsigned char kNoOutputArg = 0xff;
// At most two declared outputs per call, like the upstream contract.
inline constexpr std::size_t kMaxCompareOutputs = 2;
// The widest snapshot a declaration compares; caller-sized buffers are
// compared within this bound.
inline constexpr std::size_t kMaxSnapshotBytes = 64;

// Which caller arguments are declared outputs. A provider table would name
// these per (provider, slot); this tree has no providers, so callers build
// the contract directly. Plain data, no handles.
struct OutputContract {
  unsigned char first = kNoOutputArg;   // argument index of output 0
  unsigned char second = kNoOutputArg;  // argument index of output 1, or kNoOutputArg
};

using CompareSnapshot = std::array<unsigned char, kMaxSnapshotBytes>;

enum class CompareOutcome {
  Unknown,     // no declared contract: nothing was read
  Unreadable,  // a contract exists but no declared output could be compared
  Same,        // every declared output was compared and held the same value
  Changed,     // at least one declared output differs
};

struct CompareReport {
  CompareOutcome outcome = CompareOutcome::Unknown;
  std::size_t changedCount = 0;
  std::array<std::size_t, kMaxCompareOutputs> changed = {};  // output indices (0/1)
  bool partial = false;  // some declared output could not be compared
};

// Decide what the before/after snapshots show. `compared[i]` says output
// i's bytes were captured; `bytes[i]` is the resolved width to compare
// (0 and uncompared outputs are skipped; widths past the snapshot bound
// compare within it). A null contract reads nothing and reports Unknown.
// Never throws.
CompareReport decideCompare(
    const OutputContract* contract,
    const std::array<bool, kMaxCompareOutputs>& compared,
    const std::array<CompareSnapshot, kMaxCompareOutputs>& before,
    const std::array<CompareSnapshot, kMaxCompareOutputs>& after,
    const std::array<std::size_t, kMaxCompareOutputs>& bytes);

// Render the report: "?", "unreadable", "same", "changed:<arg>" or
// "changed:<arg0>,<arg1>", with ",partial" appended when some declared
// output could not be compared. Argument numbers are the caller's own
// (index 0 lists like any other). A null contract renders "?". Never
// throws.
std::string formatCompare(const CompareReport& report, const OutputContract* contract);

}  // namespace diag
}  // namespace sb
