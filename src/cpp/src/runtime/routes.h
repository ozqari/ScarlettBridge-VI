#pragma once
// HLE route table: which imported DLL names bind to which of our modules,
// which pass through to the host, and which become automatic stubs.
//
// Idea informed by xcl-dev/xcl hle/mod.rs (Registry routes with user
// aliases first, canonical kernel/xgameruntime/d3d12 routes, glob patterns,
// Route::Hle/System/Stub, hle_for, find_any cross-module lookup,
// aliases_are_system via "@self" fallback) and victory-fork registry.rs
// (HleModule::serves with trailing-'*' prefix, export uniqueness per
// module, api-set prefix test). Independently implemented here in portable
// C++; no code copied, no OS loading.
#include <optional>
#include <string>
#include <vector>

namespace sb {

enum class RouteKind { Hle, System, Stub };

struct RouteEntry {
  std::string pattern;       // lower-cased glob ( '*' matches any run )
  RouteKind kind = RouteKind::Stub;
  std::string hleCanonical;  // lower-cased canonical DLL for Hle, else empty
};

// Case-insensitive glob: '*' matches any (possibly empty) run of chars.
bool globMatch(const std::string& pattern, const std::string& value);
std::string dllKeyOf(const std::string& dll);

class RouteTable {
 public:
  // Appends a canonical route (first match wins, so user aliases must be
  // added before defaults — same precedence as upstream build()).
  void addRoute(const std::string& pattern, RouteKind kind,
                const std::string& hleCanonical = "");
  // User alias target: "@system", "@stub", or a canonical module DLL name.
  // Unknown HLE targets are ignored (warned by the caller).
  void addAlias(const std::string& pattern, const std::string& target);
  // First matching route for a DLL key, if any.
  std::optional<RouteEntry> route(const std::string& dll) const;
  // Canonical HLE module a DLL routes to, if the route is Hle.
  std::optional<std::string> hleFor(const std::string& dll) const;

  // Module export coverage with per-module uniqueness (duplicates rejected,
  // like victory registry's export_names_are_unique_per_dll_pattern test).
  // Returns false when `exports` contains a duplicate (table unchanged).
  bool registerModule(const std::string& canonical,
                      const std::vector<std::string>& exports);
  // Cross-module lookup for console functions whose hosting DLL is unknown
  // (e.g. XMem*): exact symbol match anywhere. Ordinals ("#n") never match:
  // they are DLL-specific.
  std::optional<std::string> findAny(const std::string& symbol) const;

 private:
  std::vector<RouteEntry> routes_;
  std::vector<std::pair<std::string, std::vector<std::string>>> modules_;
};

// Default canonical routes mirroring upstream build(): kernel family,
// xgameruntime family, d3d12 family, audio/media, user, storage, texture,
// controllers, compiler shims. Narrow, reviewable, and boring on purpose.
RouteTable defaultRoutes();

}  // namespace sb
