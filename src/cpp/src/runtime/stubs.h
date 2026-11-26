#pragma once
// Stub database: default return values + ordinal names for functions we do
// not implement yet, with per-stub call counting.
//
// Idea informed by xcl-dev/xcl crates/xcl/src/stubs.rs (independently
// implemented, no code copied): a stub table maps `dll!name` (case-insensitive)
// to a configured return value, with module-level defaults, a `*!name`
// wildcard, ordinal-name lookup, and first-caller tracking so the first call
// of each stub is logged once instead of flooding the log.
//
// What we deliberately do NOT copy: XCL emits executable x64 thunks into a
// VirtualAlloc arena and calls them. We never execute foreign code and never
// emit machine code. A "stub" here is a record: resolve() still fails the
// load (never fakes success); the database only decides the reported return
// value, counts calls for diagnostics, and names ordinals in reports.
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace sb {

struct StubCallInfo {
  std::string dll;
  std::string name;
  std::uint64_t configuredRet = 0;
  std::uint64_t calls = 0;
  std::uint64_t firstCaller = 0;
};

class StubDatabase {
 public:
  void setModuleDefault(const std::string& dll, std::uint64_t ret);
  void setFunctionReturn(const std::string& dll, const std::string& name, std::uint64_t ret);
  void setOrdinalName(const std::string& dll, std::uint16_t ordinal, const std::string& name);

  // Configured return for dll!name. Precedence: exact function, `*!name`
  // wildcard, module default, fallback-module exact/default, else 0.
  // `fallbackDll` names the canonical HLE module when `dll` is a routed alias.
  std::uint64_t returnFor(const std::string& dll, const std::string& name,
                          const std::string& fallbackDll = "") const;
  std::string ordinalName(const std::string& dll, std::uint16_t ordinal) const;

  // Count a call; records the first caller once. Returns the configured value.
  std::uint64_t recordCall(const std::string& dll, const std::string& name,
                           std::uint64_t caller);

  std::vector<StubCallInfo> snapshot() const;

 private:
  static std::string key(const std::string& dll, const std::string& name);
  static std::string lower(std::string s);

  mutable std::mutex mu_;
  std::map<std::string, std::uint64_t> moduleDefaults_;   // dll-lower -> ret
  std::map<std::string, std::uint64_t> functions_;        // "dll!name"-lower -> ret
  std::map<std::string, std::map<std::uint16_t, std::string>> ordinals_;
  std::map<std::string, StubCallInfo> calls_;  // key -> info
};

}  // namespace sb
