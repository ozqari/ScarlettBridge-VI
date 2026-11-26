#pragma once
// Explicit import registry with a host pass-through policy and a census
// classifier, modeled on the resolution order documented in victory-fork's
// docs/architecture.md (our implementation, their policy idea):
//   1. our own service implementation (always wins);
//   2. a host function, but ONLY if HostPolicy allows that host module;
//   3. otherwise an explicit stub/failure — never a silent success.
//
// Stub modes: Trap (abort with module+symbol+RVA-style detail) or LogAndFail
// (log once, fail the load). There is deliberately NO "return zero and
// continue" mode: faking success for unknown functions is how hangs and
// corrupt state hide. census() classifies every import WITHOUT running
// anything, like `victory census`.
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {

struct UnresolvedImport {
  std::string module;
  std::string symbol;
  std::string severity = "fatal";  // unknown imports are always fatal here
};

enum class StubMode { Trap, LogAndFail };

// Default-deny pass-through policy for host modules (rule 2).
// Patterns are case-insensitive; a trailing '*' matches a prefix, which
// covers versioned API sets (e.g. "api-ms-win-core-*"). Empty = nothing passes.
// Idea informed by victory-fork crates/victory-hle/src/host.rs (HostPolicy)
// and xcl hle routing (glob routes + aliases), independently implemented.
class HostPolicy {
 public:
  // Adds one allow pattern: an exact DLL name or a prefix ending in '*'.
  void allowHostModule(const std::string& pattern);
  bool allows(const std::string& hostModule) const;

 private:
  std::vector<std::string> patterns_;  // stored lower-cased
};

enum class ImportClass {
  Ours,        // resolved to our own service implementation
  HostAllowed, // would pass through to the host (policy allows)
  HostDenied,  // host module exists in principle but policy denies it
  StubTrap,    // no implementation: would trap with module+symbol detail
  Missing      // no implementation and no stub rule: load fails
};

struct CensusEntry {
  std::string module;
  std::string symbol;
  ImportClass klass = ImportClass::Missing;
  // Honesty fields (idea from Xbox2Windows tools/import_routing.py, which
  // labels every row route_kind="candidate", resolution="not_checked"):
  // this census classifies module names only. It never resolves exports,
  // proves a provider, or loads anything.
  std::string routeKind = "candidate";
  std::string resolution = "not_checked";
  std::string provider;  // set for Ours (e.g. "hle:kernel"), else empty
};

std::string toString(ImportClass c);

class ImportRegistry {
 public:
  explicit ImportRegistry(StubMode stub = StubMode::LogAndFail) : stub_(stub) {}
  void registerService(const std::string& name, std::uint16_t version);
  // Our own HLE coverage for a host-DLL import: dll match is
  // case-insensitive, symbol match is exact ("#123" for ordinals).
  // Idea from victory-fork registry.rs (HleModule::serves + lookup) and XCL
  // cross-module find_any; independently implemented here as a plain table.
  void registerHle(const std::string& dll, const std::string& symbol);
  bool hasHle(const std::string& dll, const std::string& symbol) const;
  // Any HLE symbol at all for this DLL (used for LoadLibrary routing).
  bool hasAnyHle(const std::string& dll) const;
  // DLL (lower-cased) providing `symbol` from any module, if exactly one or
  // more modules serve it (first in key order). Empty symbol or ordinal
  // ("#n") never matches: ordinals are DLL-specific.
  std::optional<std::string> findHleProvider(const std::string& symbol) const;
  const HostPolicy& hostPolicy() const { return hostPolicy_; }
  void setHostPolicy(HostPolicy policy) { hostPolicy_ = std::move(policy); }
  void setStubMode(StubMode m) { stub_ = m; }
  // Returns the registered version, or records an unresolved import + error.
  Result<std::uint16_t> resolve(const std::string& module, const std::string& name,
                                std::uint16_t minVersion);
  // Classify without recording failures or resolving anything.
  CensusEntry census(const std::string& module, const std::string& name) const;
  // DLL-style census (PE import rows): resolution order is our HLE table,
  // then the host policy for that DLL name, then Missing/StubTrap.
  // Mirrors the order documented in victory-hle lib.rs (modules, host, Missing).
  CensusEntry censusDll(const std::string& dll, const std::string& symbol) const;
  const std::vector<UnresolvedImport>& unresolved() const { return unresolved_; }
  void clearUnresolved() { unresolved_.clear(); }

 private:
  static bool isHostModuleName(const std::string& dll);
  std::map<std::string, std::uint16_t> services_;
  std::map<std::string, std::set<std::string>> hle_;  // dll-lower -> symbols
  HostPolicy hostPolicy_;
  StubMode stub_ = StubMode::LogAndFail;
  std::vector<UnresolvedImport> unresolved_;
};

}  // namespace sb
