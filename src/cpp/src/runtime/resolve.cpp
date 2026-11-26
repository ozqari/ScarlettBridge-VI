#include "runtime/resolve.h"

#include <cctype>

namespace sb {
namespace {

std::string fileNameOf(const std::string& path) {
  std::size_t cut = path.find_last_of("/\\");
  return cut == std::string::npos ? path : path.substr(cut + 1);
}

}  // namespace

std::string toString(ResolvedSource s) {
  switch (s) {
    case ResolvedSource::Hle: return "hle";
    case ResolvedSource::HleOther: return "hle-other";
    case ResolvedSource::GameDll: return "gamedll";
    case ResolvedSource::System: return "system";
    case ResolvedSource::Stub: return "stub";
  }
  return "?";
}

std::string ImportResolver::lowerKey(const std::string& s) {
  std::string out = s;
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

std::optional<std::string> ImportResolver::findGameDll(const std::string& name) const {
  const std::string want = lowerKey(fileNameOf(name));
  if (want.empty()) return std::nullopt;
  for (const auto& f : gameFiles_) {
    if (lowerKey(fileNameOf(f)) == want) return f;
  }
  return std::nullopt;
}

ResolvedSource ImportResolver::classify(const std::string& dll,
                                       const std::string& symbol) const {
  if (registry_ && registry_->hasHle(dll, symbol)) return ResolvedSource::Hle;
  if (findGameDll(dll).has_value()) return ResolvedSource::GameDll;
  if (registry_) {
    // System needs both: policy allows the DLL and the export is present.
    // The export set is injected (tests) or filled from real GetProcAddress
    // probes (Windows builds); unknown exports never count as system.
    std::string key = lowerKey(dll) + "!" + symbol;
    if (systemExports_.count(key) != 0) {
      // Policy check via censusDll: HostAllowed means allowed.
      auto c = registry_->censusDll(dll, symbol);
      if (c.klass == ImportClass::HostAllowed) return ResolvedSource::System;
    }
  }
  // Known console function imported from an unexpected DLL (named only).
  // Raw ordinals are DLL-specific: never resolve one DLL's ordinal in
  // another DLL.
  if (!isOrdinal(symbol) && registry_) {
    if (auto provider = registry_->findHleProvider(symbol); provider.has_value())
      return ResolvedSource::HleOther;
  }
  return ResolvedSource::Stub;
}

ResolvedSource ImportResolver::resolveImport(const std::string& importer, const std::string& dll,
                                            const std::string& symbol, bool delay) {
  ResolvedSource source = classify(dll, symbol);
  records_.push_back(ImportRecord{importer, lowerKey(dll), symbol, delay, source});
  return source;
}

std::optional<ResolvedSource> ImportResolver::getProcAddress(const std::string& dll,
                                                           const std::string& symbol) const {
  if (registry_ && registry_->hasHle(dll, symbol)) return ResolvedSource::Hle;
  if (findGameDll(dll).has_value()) return ResolvedSource::GameDll;
  if (registry_) {
    std::string key = lowerKey(dll) + "!" + symbol;
    if (systemExports_.count(key) != 0) {
      if (registry_->censusDll(dll, symbol).klass == ImportClass::HostAllowed)
        return ResolvedSource::System;
    }
  }
  if (!isOrdinal(symbol) && registry_) {
    if (registry_->findHleProvider(symbol).has_value()) return ResolvedSource::HleOther;
  }
  // Missing: null, never a stub (optional-API probing relies on this).
  return std::nullopt;
}

std::optional<std::string> ImportResolver::loadLibrary(const std::string& name) const {
  if (registry_) {
    if (registry_->hasAnyHle(name)) return std::string("hle");
  }
  if (findGameDll(name).has_value()) return std::string("gamedll");
  if (registry_) {
    // System route for LoadLibrary: policy allows the name. No export check
    // here: the module itself is what is requested.
    if (registry_->censusDll(name, "LoadLibraryProbe").klass == ImportClass::HostAllowed)
      return std::string("system");
  }
  return std::nullopt;
}

}  // namespace sb
