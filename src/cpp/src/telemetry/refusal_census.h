#pragma once
// Bounded refusal accounting for translated draws.
//
// Idea informed by Xbox2Windows docs/startup-runtime.md ("Null bindings in
// translated draws", "Per-reason refusal census per pipeline key") and the
// fetched headers src/startup_stubs/null_binding_cause.h (five causes that
// sum to the null total) and per_reason_refusal_census.h (128-entry
// per-(key,reason) table, 64-byte sanitized reasons, first-8 detail budget,
// delta reports, overflow counting). Independently implemented here in
// portable C++ with our own types and wording; no code copied, no OS calls,
// no game content. Unit-tested in sb_tests.
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace sb {
namespace diag {

// Why one translated draw bound a null descriptor. Exactly one cause per
// null binding, so the per-cause counts always sum to the null total.
enum class NullCause : int {
  None = -1,
  Entry = 0,      // table entry null, empty, or unreadable
  Buffer = 1,     // buffer range missing/unreadable, or its frame copy failed
  Token = 2,      // descriptor memory names a view the wrapper never issued
  Wrapper = 3,    // the view's resource is missing or not a resource wrapper
  Dimension = 4,  // the view dimension does not suit the shader
};

const char* nullCauseName(NullCause c);
inline constexpr unsigned kNullCauses = 5;
inline unsigned nullCauseCount() { return kNullCauses; }

// Facts a draw records about one binding. Plain data, no handles.
struct BindingFacts {
  bool isTexture = false;
  bool fromTable = false;       // descriptor came from a descriptor-table entry
  bool entryReadable = false;   // that entry was present and readable
  bool tokenMagic = false;      // its first word marks a wrapper-made view
  bool tokenKnown = false;      // and the view id is one the wrapper issued
  bool resourceIsWrapper = false;  // the view resolves to a resource wrapper
  bool dimensionMatches = true;    // the view dimension suits the shader
  bool rangeBound = false;         // a readable buffer range was bound
};

// Pure decision function, unit-tested.
NullCause classifyNullBinding(const BindingFacts& f);

// Running per-cause counters with a consistency check.
struct NullCensus {
  std::uint64_t counts[kNullCauses] = {};
  void note(NullCause c);
  std::uint64_t total() const;
  bool consistent() const { return true; }  // sums by construction; see total()
};

// Bounded per-(pipeline key, reason) refusal table. Later refusals on the
// same key never overwrite earlier reasons: each distinct pair keeps its own
// count. Past the cap, calls only advance the total and the overflow count.
class PerReasonCensus {
 public:
  static constexpr unsigned kMaxEntries = 128;
  static constexpr unsigned kDetailBudget = 8;
  static constexpr std::size_t kMaxReason = 64;
  static constexpr std::size_t kMaxLabel = 32;

  explicit PerReasonCensus(const std::string& label = "per-reason");

  // Counts one refusal for (key, reason). Returns true when the caller
  // should emit a detail log: the first insertion of one of the first
  // kDetailBudget distinct pairs.
  bool note(std::uint64_t key, const std::string& reason);

  std::uint64_t total() const;
  unsigned entries() const;
  unsigned keys() const;
  std::uint64_t overflow() const;

  // Summary line plus one line per entry (insertion order). With
  // onlyIfChanged, entries whose counts have not grown since the last such
  // report are skipped, and an unchanged table yields no lines at all.
  // Never throws.
  std::vector<std::string> report(bool onlyIfChanged);

  static std::string sanitizeReason(const std::string& s);

 private:
  struct Entry {
    std::uint64_t key = 0;
    std::string reason;
    std::uint64_t calls = 0;
    std::uint64_t reported = 0;
  };
  bool hasKeyLocked(std::uint64_t key) const;

  mutable std::mutex mu_;
  std::string label_;
  std::vector<Entry> entries_;
  unsigned distinctKeys_ = 0;
  std::uint64_t total_ = 0;
  std::uint64_t reported_ = 0;
  std::uint64_t overflow_ = 0;
};

}  // namespace diag
}  // namespace sb
