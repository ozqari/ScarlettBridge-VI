#pragma once
// First-query-only tracing + import-trace filter.
//
// Idea informed by AnyXboxX runtime/xgameruntime/xgameruntime.cpp
// (FirstQuery: a hot interface is logged once per provider/iid pair, a 128
// entry table, table-full stays silent) and xcl-dev/xcl resolver.rs
// TraceFilter (XCL_TRACE_IMPORTS: "1"/"hle", "all", prefix includes,
// "-prefix" excludes; stubs never traced). Independently implemented here
// in portable C++; no code copied.
#include <cstddef>
#include <string>
#include <vector>

namespace sb {
namespace diag {

// Remembers up to `capacity` distinct keys; first() is true exactly once
// per key. When the table is full, new keys return false (stay silent
// rather than flood the trace).
class FirstSeenThrottle {
 public:
  explicit FirstSeenThrottle(std::size_t capacity = 128) : cap_(capacity) {}
  bool first(const std::string& a, const std::string& b);
  bool first(const std::string& key);
  std::size_t size() const { return seen_.size(); }
  void clear() { seen_.clear(); }

 private:
  std::size_t cap_ = 128;
  std::vector<std::string> seen_;
};

enum class TraceSource { Hle, HleOther, GameDll, System, Stub };

// Comma-separated filter in the XCL_TRACE_IMPORTS shape: "1"/"hle" traces
// every HLE call (the default when no includes are given), "all" also
// traces system calls, any other word is a "dll!function" prefix to
// include, "-word" excludes. Matching is lower-case prefix on the full
// "dll!function" name. Stubs never trace (they log their own first call).
struct TraceFilter {
  bool hle = true;
  bool all = false;
  std::vector<std::string> include;
  std::vector<std::string> exclude;

  static TraceFilter parse(const std::string& env);
  bool wants(const std::string& dll, const std::string& function, TraceSource source) const;

 private:
  static std::string lower(const std::string& s);
};

}  // namespace diag
}  // namespace sb
