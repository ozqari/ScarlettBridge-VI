#pragma once
// Portable failure tally for offline shader batches.
//
// Behavior informed by victory-fork `apps/victory/src/translate.rs`
// (Tally::fail/merge: reason -> (count, first example), list-mode
// "<example>  <reason>" lines, merge sums counts and keeps the first
// example, failure report sorted most-frequent-first). Independently
// implemented here in portable C++ with our own types and wording; the
// translator stage counters (containers/programs/emitted/validated/...
// tallies) are NOT implemented — this tree has no batch translator run,
// so only the failure accounting travels. Unit-tested in sb_tests.
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace sb {
namespace gfx {

// Reason -> programs stopped by it, and the first one. Plain data, no
// threads: a batch runner folds per-thread tallies with merge().
class FailTally {
 public:
  // With list=true every fail() also records an "<example>  <reason>"
  // line (the batch's --list mode: one line per program and outcome).
  explicit FailTally(bool list = false);

  bool list() const;

  // Count one program stopped by `reason`, naming `example`. The first
  // example for a reason wins; later ones only raise the count. Never
  // throws.
  void fail(const std::string& reason, const std::string& example);

  // Fold `other` in: counts summed, first examples kept (ours win on a
  // clash), list lines appended. Never throws.
  void merge(const FailTally& other);

  std::size_t total() const;    // programs counted, all reasons
  std::size_t reasons() const;  // distinct reasons

  // (count, first example) for `reason`, or (0, "") when unseen. Never
  // throws.
  std::pair<std::size_t, std::string> countFor(const std::string& reason) const;

  // List-mode lines, sorted ascending (the batch prints them sorted).
  // Empty unless built with list=true. Never throws.
  std::vector<std::string> listLines() const;

  // Failure report, most frequent first (ties keep reason-ascending
  // order): "  <count:7>  <reason>  (e.g. <example>)". Empty when nothing
  // failed. Never throws.
  std::vector<std::string> report() const;

 private:
  bool list_ = false;
  std::size_t total_ = 0;
  // Ordered by reason (the upstream map is a BTreeMap), so tied counts
  // report in reason-ascending order exactly like its stable sort does.
  std::map<std::string, std::pair<std::size_t, std::string>> failed_;
  std::vector<std::string> lines_;
};

}  // namespace gfx
}  // namespace sb
