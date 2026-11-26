#pragma once
// Versioned on-disk shader cache. Key = SHA-256 over
// ("sbpc1" | translatorVersion | backend | shaderHash | layoutHash).
// Corrupt entries are discarded (reported) and never used. Bounded by
// entry count and total bytes; over-budget stores fail explicitly.
//
// Key v2 ("sbpc2" | translatorFingerprint | backend | containerHash) is the
// hardened key: the translator fingerprint (its version + encoding, kept in
// this file so a translator change invalidates old entries) plus our own
// encoding bytes are part of the key, so a changed translator never serves
// an old translation. Idea from victory-gpu shader_cache.rs build(); the
// encoding here is our own SBPC entry format.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {
namespace cache {

std::string entryKey(const std::string& backend, const std::string& shaderHash,
                     const std::string& layoutHash);

// Fingerprint identifying the translation, besides the container: translator
// + cache-entry versions and this file's encoding tag. Any change here (or
// to the versions in core/version.h) retires old cache entries.
std::string translatorFingerprint();
std::string cacheKeyV2(const std::string& translatorFp, const std::string& backend,
                       const std::string& containerHash);

struct Entry {
  std::string key;
  std::vector<std::uint8_t> payload;
};

struct CacheCounters {
  std::uint64_t hits = 0;
  std::uint64_t misses = 0;
};

class ShaderCache {
 public:
  ShaderCache(std::string dir, std::size_t maxEntries, std::size_t maxBytes)
      : dir_(std::move(dir)), maxEntries_(maxEntries), maxBytes_(maxBytes) {}
  Result<Entry> load(const std::string& key);   // miss OR corrupt-discard are both explicit
  bool wasCorruptDiscard() const { return corruptDiscard_; }
  Result<void> store(const std::string& key, const std::vector<std::uint8_t>& payload);
  Result<void> clear();  // removes only this cache dir's *.bin entries
  Result<void> stats(std::size_t& entries, std::size_t& bytes);
  // Offline warm / get-or-translate: `translate` runs only on a miss or an
  // unreadable entry (which is removed and retried once, like upstream's
  // two-attempt loop). Hits and misses are counted in `counters`.
  Result<std::vector<std::uint8_t>> getOrTranslate(
      const std::string& key,
      const std::function<Result<std::vector<std::uint8_t>>()>& translate,
      CacheCounters& counters);

 private:
  std::string dir_;
  std::size_t maxEntries_ = 4096;
  std::size_t maxBytes_ = 0;
  bool corruptDiscard_ = false;
};

}  // namespace cache
}  // namespace sb
