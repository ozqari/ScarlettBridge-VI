#pragma once
// Portable engine-counter aggregation for diagnostics.
//
// Behavior informed by victory-fork `crates/victory-gpu/src/stats.rs`
// (parse_instance: "pid_<pid>_..._engtype_<kind>" names the process and the
// engine kind; busiest: every kind's figures summed, the highest sum wins at
// 100 at most, whole card or one process). Independently implemented here in
// portable C++ with our own types and wording; the OS collection behind it
// (PDH counters, GetSystem/Process/ThreadTimes, memory queries, the 1 s
// sampling thread) is NOT implemented — this tree has no OS stat collection,
// so these pure helpers aggregate caller-supplied samples only.
// Unit-tested in sb_tests.
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sb {
namespace diag {

// One engine figure: the owning process, the engine kind ("3D", "Copy",
// "Compute_0", ...), and its per-cent share. Plain data, no handles.
struct EngineSample {
  std::uint32_t pid = 0;
  std::string kind;
  double percent = 0.0;
};

// What an instance name says: the process and the engine kind, e.g.
// "pid_1234_luid_0x00000000_0x0000E3B5_phys_0_eng_0_engtype_3D" -> (1234,
// "3D"). nullopt when the "pid_" prefix, the numeric pid, or "engtype_" is
// missing. A repeated "engtype_" ends the kind at its next occurrence (same
// cut as the upstream split().nth(1) read). Never throws.
std::optional<std::pair<std::uint32_t, std::string>> parseEngineInstance(
    const std::string& name);

// Per-kind sums plus the winning sum, for the whole card (pid nullopt) or
// one process. The winner is the highest per-kind sum, 100 at most, 0 when
// nothing matches. Deterministic key order (std::map). Never throws.
struct BusiestEngines {
  float top = 0.0f;
  std::map<std::string, double> byKind;
};
BusiestEngines busiestEngineKind(const std::vector<EngineSample>& engines,
                                 std::optional<std::uint32_t> pid);

}  // namespace diag
}  // namespace sb
