#include "runtime/routes.h"

#include <cctype>

namespace sb {
namespace {

std::string lowerStr(const std::string& s) {
  std::string o = s;
  for (char& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return o;
}

}  // namespace

std::string dllKeyOf(const std::string& dll) { return lowerStr(dll); }

bool globMatch(const std::string& pattern, const std::string& value) {
  const std::string p = lowerStr(pattern);
  const std::string v = lowerStr(value);
  std::size_t pi = 0, vi = 0, star = std::string::npos, mark = 0;
  while (vi < v.size()) {
    if (pi < p.size() && (p[pi] == v[vi])) {
      ++pi;
      ++vi;
    } else if (pi < p.size() && p[pi] == '*') {
      star = pi++;
      mark = vi;
    } else if (star != std::string::npos) {
      pi = star + 1;
      vi = ++mark;
    } else {
      return false;
    }
  }
  while (pi < p.size() && p[pi] == '*') ++pi;
  return pi == p.size();
}

void RouteTable::addRoute(const std::string& pattern, RouteKind kind,
                           const std::string& hleCanonical) {
  RouteEntry e;
  e.pattern = lowerStr(pattern);
  e.kind = kind;
  e.hleCanonical = lowerStr(hleCanonical);
  routes_.push_back(std::move(e));
}

void RouteTable::addAlias(const std::string& pattern, const std::string& target) {
  std::string t = lowerStr(target);
  // Trim ASCII whitespace the way upstream trims alias targets.
  while (!t.empty() && (t.front() == ' ' || t.front() == '\t')) t.erase(t.begin());
  while (!t.empty() && (t.back() == ' ' || t.back() == '\t')) t.pop_back();
  RouteEntry e;
  e.pattern = lowerStr(pattern);
  if (t == "@system") {
    e.kind = RouteKind::System;
  } else if (t == "@stub") {
    e.kind = RouteKind::Stub;
  } else {
    bool known = false;
    for (const auto& m : modules_) {
      if (m.first == t) {
        known = true;
        break;
      }
    }
    if (!known) return;  // unknown target: skip, caller warns
    e.kind = RouteKind::Hle;
    e.hleCanonical = t;
  }
  // User aliases take precedence over canonical defaults: front-insert, so
  // the most recently added alias wins first (same order as upstream's
  // build(), which pushes all aliases before defaults).
  routes_.insert(routes_.begin(), std::move(e));
}

std::optional<RouteEntry> RouteTable::route(const std::string& dll) const {
  const std::string key = lowerStr(dll);
  for (const auto& r : routes_) {
    if (globMatch(r.pattern, key)) return r;
  }
  return std::nullopt;
}

std::optional<std::string> RouteTable::hleFor(const std::string& dll) const {
  auto r = route(dll);
  if (!r.has_value() || r->kind != RouteKind::Hle) return std::nullopt;
  return r->hleCanonical;
}

bool RouteTable::registerModule(const std::string& canonical,
                                 const std::vector<std::string>& exports) {
  std::string key = lowerStr(canonical);
  for (std::size_t i = 0; i < exports.size(); ++i) {
    for (std::size_t j = i + 1; j < exports.size(); ++j) {
      if (exports[i] == exports[j]) return false;  // duplicate export
    }
  }
  for (auto& m : modules_) {
    if (m.first == key) {
      m.second = exports;
      return true;
    }
  }
  modules_.push_back({key, exports});
  return true;
}

std::optional<std::string> RouteTable::findAny(const std::string& symbol) const {
  if (symbol.empty() || symbol[0] == '#') return std::nullopt;
  for (const auto& m : modules_) {
    for (const auto& e : m.second) {
      if (e == symbol) return m.first;
    }
  }
  return std::nullopt;
}

RouteTable defaultRoutes() {
  RouteTable t;
  // Canonical modules (names only; exports are registered by tests/CLI —
  // the table never fabricates coverage).
  for (const char* m : {"kernelx.dll", "xgameruntime.dll", "d3d12_xs.dll", "xaudio2_9.dll",
                        "mfplat.dll", "dxcompiler_xs.dll", "minuserx.dll", "xcrdapi.dll",
                        "xg_xs.dll", "acphal.dll", "gameinput.dll"}) {
    t.registerModule(m, {});
  }
  // Kernel family first (legacy + api sets route to kernelx).
  for (const char* p : {"kernelx.dll", "kernel32.dll", "kernelbase.dll", "advapi32.dll",
                        "kernel32legacy.dll", "advapi32legacy.dll", "minappmodelx.dll",
                        "xmem.dll", "api-ms-win-core-*", "ext-ms-win-core-*"}) {
    t.addRoute(p, RouteKind::Hle, "kernelx.dll");
  }
  for (const char* p : {"xgameruntime.dll", "xgameruntime*.dll", "xgameplatform.dll"})
    t.addRoute(p, RouteKind::Hle, "xgameruntime.dll");
  for (const char* p : {"d3d12_xs.dll", "d3d12_x.dll", "d3d12x.dll"})
    t.addRoute(p, RouteKind::Hle, "d3d12_xs.dll");
  for (const char* p : {"xaudio2_9.dll", "xaudio2_9redist.dll"})
    t.addRoute(p, RouteKind::Hle, "xaudio2_9.dll");
  t.addRoute("mfplat.dll", RouteKind::Hle, "mfplat.dll");
  t.addRoute("minuserx.dll", RouteKind::Hle, "minuserx.dll");
  t.addRoute("ext-ms-win-rtcore-ntuser-window-ansi-*", RouteKind::Hle, "minuserx.dll");
  t.addRoute("ext-ms-win-rtcore-ntuser-window-l1-*", RouteKind::Hle, "minuserx.dll");
  t.addRoute("xcrdapi.dll", RouteKind::Hle, "xcrdapi.dll");
  t.addRoute("xg_xs.dll", RouteKind::Hle, "xg_xs.dll");
  for (const char* p : {"acphal.dll", "xapu.dll", "chad.dll"})
    t.addRoute(p, RouteKind::Hle, "acphal.dll");
  t.addRoute("gameinput.dll", RouteKind::Hle, "gameinput.dll");
  for (const char* p : {"dxcompiler_xs.dll", "dxcompiler_x.dll"})
    t.addRoute(p, RouteKind::Hle, "dxcompiler_xs.dll");
  return t;
}

}  // namespace sb
