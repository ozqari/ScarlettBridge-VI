#pragma once
// Dry-run import resolution log: which provider each dll!symbol would bind
// to, without mapping or executing anything.
//
// Idea informed by xcl-dev/xcl crates/xcl/src/resolver.rs (Source
// Hle/HleOther/System/GameDll/Stub + ImportRecord + classify() that never
// maps game DLLs + ordinal-only cross-DLL refusal + GetProcAddress returning
// null on missing instead of a stub). Independently implemented here in
// portable C++ over our own ImportRegistry; no code copied, no OS loading.
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "runtime/imports.h"

namespace sb {

enum class ResolvedSource { Hle, HleOther, GameDll, System, Stub };

std::string toString(ResolvedSource s);

struct ImportRecord {
  std::string importer;
  std::string dll;       // lower-cased key form
  std::string function;  // "#n" for ordinals
  bool delay = false;
  ResolvedSource source = ResolvedSource::Stub;
};

// Portable dry-run resolver. Game files and system exports are injected as
// data (never touched on disk here) so the classifier is unit-testable:
//   gameFiles: file paths the title ships (matched case-insensitively).
//   searchDirs: folders those files were found in (informational only).
//   systemExports: "dll!symbol" entries the host is assumed to export
//     (lower-cased dll, exact symbol). A real build fills this from
//     GetProcAddress probes; tests inject a small set.
class ImportResolver {
 public:
  explicit ImportResolver(const ImportRegistry* registry) : registry_(registry) {}

  void setSearchDirs(std::vector<std::string> dirs) { searchDirs_ = std::move(dirs); }
  void setGameFiles(std::vector<std::string> files) { gameFiles_ = std::move(files); }
  void setSystemExports(std::set<std::string> exports) { systemExports_ = std::move(exports); }

  // File name (after / or \) of the game DLL `name`, if shipped. Case-
  // insensitive; absolute or bare names both match by file name.
  std::optional<std::string> findGameDll(const std::string& name) const;

  // Classify without recording or loading. Order: HLE exact, game DLL,
  // system (policy + export present), HLE-other (named symbols only),
  // else Stub. Ordinals ("#n") never resolve cross-DLL.
  ResolvedSource classify(const std::string& dll, const std::string& symbol) const;

  // Classify + append an ImportRecord. Never loads or maps.
  ResolvedSource resolveImport(const std::string& importer, const std::string& dll,
                              const std::string& symbol, bool delay);

  // GetProcAddress contract: missing functions return nullopt (null), never
  // a stub, so optional-API probing keeps working. Order: HLE exact,
  // game DLL, system, HLE-other (named only).
  std::optional<ResolvedSource> getProcAddress(const std::string& dll,
                                              const std::string& symbol) const;

  // LoadLibrary contract: which handle kind `name` would get. Returns
  // "hle", "gamedll", "system", or nullopt when nothing provides it.
  std::optional<std::string> loadLibrary(const std::string& name) const;

  const std::vector<ImportRecord>& records() const { return records_; }
  void clearRecords() { records_.clear(); }

  static bool isOrdinal(const std::string& symbol) {
    return symbol.size() > 1 && symbol[0] == '#';
  }
  static std::string lowerKey(const std::string& s);

 private:
  const ImportRegistry* registry_ = nullptr;
  std::vector<std::string> searchDirs_;
  std::vector<std::string> gameFiles_;
  std::set<std::string> systemExports_;  // "dll-lower!symbol"
};

}  // namespace sb
