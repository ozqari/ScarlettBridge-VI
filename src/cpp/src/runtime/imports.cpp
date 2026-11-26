#include "runtime/imports.h"

#include <cctype>

namespace sb {
namespace {
std::string lowerStr(const std::string& s) {
  std::string out = s;
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}
bool endsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}
}  // namespace

std::string toString(ImportClass c) {
  switch (c) {
    case ImportClass::Ours: return "ours";
    case ImportClass::HostAllowed: return "host-allowed";
    case ImportClass::HostDenied: return "host-denied";
    case ImportClass::StubTrap: return "stub-trap";
    case ImportClass::Missing: return "missing";
  }
  return "?";
}

void HostPolicy::allowHostModule(const std::string& pattern) {
  patterns_.push_back(lowerStr(pattern));
}

bool HostPolicy::allows(const std::string& hostModule) const {
  const std::string dll = lowerStr(hostModule);
  for (const auto& pat : patterns_) {
    if (!pat.empty() && pat.back() == '*') {
      if (dll.compare(0, pat.size() - 1, pat, 0, pat.size() - 1) == 0) return true;
    } else if (dll == pat) {
      return true;
    }
  }
  return false;
}

void ImportRegistry::registerService(const std::string& name, std::uint16_t version) {
  services_[name] = version;
}

void ImportRegistry::registerHle(const std::string& dll, const std::string& symbol) {
  hle_[lowerStr(dll)].insert(symbol);
}

bool ImportRegistry::hasHle(const std::string& dll, const std::string& symbol) const {
  auto it = hle_.find(lowerStr(dll));
  return it != hle_.end() && it->second.count(symbol) != 0;
}

bool ImportRegistry::hasAnyHle(const std::string& dll) const {
  auto it = hle_.find(lowerStr(dll));
  return it != hle_.end() && !it->second.empty();
}

std::optional<std::string> ImportRegistry::findHleProvider(const std::string& symbol) const {
  if (symbol.empty() || symbol[0] == '#') return std::nullopt;  // ordinals are DLL-specific
  for (const auto& [dll, symbols] : hle_) {
    if (symbols.count(symbol) != 0) return dll;
  }
  return std::nullopt;
}

bool ImportRegistry::isHostModuleName(const std::string& dll) {
  const std::string d = lowerStr(dll);
  if (endsWith(d, ".dll")) return true;
  if (d.rfind("api-ms-win-", 0) == 0) return true;
  if (d.rfind("ext-ms-", 0) == 0) return true;
  return false;
}

CensusEntry ImportRegistry::census(const std::string& module,
                                   const std::string& name) const {
  if (services_.find(name) != services_.end()) {
    CensusEntry e{module, name, ImportClass::Ours};
    e.provider = name;
    return e;
  }
  // Back-compat path for synthetic SBM service names: a service-shaped name
  // with a host-module shape (foo.dll) is classified by policy; anything
  // else with no implementation cannot load.
  bool hostShaped = name.size() > 4 &&
                    (name.compare(name.size() - 4, 4, ".dll") == 0 ||
                     name.compare(name.size() - 4, 4, ".DLL") == 0);
  if (hostShaped) {
    CensusEntry e{module, name,
                  hostPolicy_.allows(name) ? ImportClass::HostAllowed
                                           : ImportClass::HostDenied};
    return e;
  }
  if (stub_ == StubMode::Trap) return CensusEntry{module, name, ImportClass::StubTrap};
  return CensusEntry{module, name, ImportClass::Missing};
}

CensusEntry ImportRegistry::censusDll(const std::string& dll,
                                      const std::string& symbol) const {
  if (hasHle(dll, symbol)) {
    CensusEntry e{dll, symbol, ImportClass::Ours};
    e.provider = "hle:" + lowerStr(dll);
    return e;
  }
  if (isHostModuleName(dll)) {
    CensusEntry e{dll, symbol, hostPolicy_.allows(dll) ? ImportClass::HostAllowed
                                                      : ImportClass::HostDenied};
    return e;
  }
  if (stub_ == StubMode::Trap) return CensusEntry{dll, symbol, ImportClass::StubTrap};
  return CensusEntry{dll, symbol, ImportClass::Missing};
}

Result<std::uint16_t> ImportRegistry::resolve(const std::string& module, const std::string& name,
                                              std::uint16_t minVersion) {
  auto it = services_.find(name);
  if (it == services_.end()) {
    // Rule 2 would consult HostPolicy for real host DLLs; the synthetic
    // loader has no host DLLs, so a policy-allowed name is still unresolvable
    // here and is reported as denied-by-environment, never faked.
    unresolved_.push_back(UnresolvedImport{module, name, "fatal"});
    return Result<std::uint16_t>::fail(
        Error::make("imports", 1, "unresolved import '" + name + "' required by '" + module +
                                    "' (stub mode: " +
                                    (stub_ == StubMode::Trap ? "trap" : "log-and-fail") + ")"));
  }
  if (it->second < minVersion) {
    unresolved_.push_back(UnresolvedImport{module, name, "fatal"});
    return Result<std::uint16_t>::fail(Error::make(
        "imports", 2, "import '" + name + "' version too old (have " +
                          std::to_string(it->second) + ", need " + std::to_string(minVersion) + ")"));
  }
  return Result<std::uint16_t>::success(it->second);
}

}  // namespace sb
