#include "graphics/pipeline_cache.h"
#include <filesystem>
#include <fstream>
#include "core/filesystem_guards.h"
#include "core/hash.h"
#include "core/version.h"

namespace sb {
namespace cache {
namespace {
bool safeKey(const std::string& k) {
  if (k.size() != 64) return false;
  for (char c : k)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}
}  // namespace

std::string entryKey(const std::string& backend, const std::string& shaderHash,
                     const std::string& layoutHash) {
  std::string material = "sbpc1|" + std::to_string(kTranslatorVersion) + "|" +
                         std::to_string(kPipelineCacheVersion) + "|" + backend + "|" +
                         shaderHash + "|" + layoutHash;
  return sha256hex(material);
}

std::string translatorFingerprint() {
  // Translator identity besides the container: versions + our own entry
  // encoding tag. Bumping any of these retires old entries without wiping
  // the whole cache by hand.
  return "sb-shader|" + std::to_string(kTranslatorVersion) + "|" +
         std::to_string(kPipelineCacheVersion) + "|sbpc-enc1";
}

std::string cacheKeyV2(const std::string& translatorFp, const std::string& backend,
                       const std::string& containerHash) {
  return sha256hex("sbpc2|" + translatorFp + "|" + backend + "|" + containerHash);
}

Result<std::vector<std::uint8_t>> ShaderCache::getOrTranslate(
    const std::string& key,
    const std::function<Result<std::vector<std::uint8_t>>()>& translate,
    CacheCounters& counters) {
  for (int attempt = 0; attempt < 2; ++attempt) {
    auto hit = load(key);
    if (hit.ok()) {
      ++counters.hits;
      return Result<std::vector<std::uint8_t>>::success(hit.value().payload);
    }
    // Corrupt entries are already removed by load(); anything else is a
    // plain miss. Either way, translate once (second attempt only reruns
    // the load after a corrupt entry was discarded).
    if (attempt == 1) break;
    if (!wasCorruptDiscard()) {
      auto made = translate();
      if (!made.ok()) return made;
      if (auto s = store(key, made.value()); !s.ok()) return Result<std::vector<std::uint8_t>>::fail(s.error());
      ++counters.misses;
      return made;
    }
  }
  // Reached only when the first load discarded a corrupt entry: translate
  // now and store the fresh bytes.
  auto made = translate();
  if (!made.ok()) return made;
  if (auto s = store(key, made.value()); !s.ok())
    return Result<std::vector<std::uint8_t>>::fail(s.error());
  ++counters.misses;
  return made;
}

Result<Entry> ShaderCache::load(const std::string& key) {
  corruptDiscard_ = false;
  if (!safeKey(key))
    return Result<Entry>::fail(Error::make("cache", 1, "bad cache key"));
  std::filesystem::path p = std::filesystem::path(dir_) / (key + ".bin");
  std::ifstream f(p, std::ios::binary);
  if (!f) return Result<Entry>::fail(Error::make("cache", 2, "cache miss"));
  std::vector<std::uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  // Format: "SBPC" u32 version u32 payloadLen payload sha256(payload, 32 bytes hex ascii).
  if (raw.size() < 4 + 4 + 4 + 64)
    return Result<Entry>::fail(Error::make("cache", 3, "entry too small"));
  if (!(raw[0] == 'S' && raw[1] == 'B' && raw[2] == 'P' && raw[3] == 'C')) {
    std::error_code ec;
    std::filesystem::remove(p, ec);
    corruptDiscard_ = true;
    return Result<Entry>::fail(Error::make("cache", 4, "bad magic, entry discarded"));
  }
  std::uint32_t ver = raw[4] | (raw[5] << 8) | (raw[6] << 16) | (raw[7] << 24);
  std::uint32_t len = raw[8] | (raw[9] << 8) | (raw[10] << 16) | (raw[11] << 24);
  if (ver != kPipelineCacheVersion || 12 + len + 64 != raw.size()) {
    std::error_code ec;
    std::filesystem::remove(p, ec);
    corruptDiscard_ = true;
    return Result<Entry>::fail(Error::make("cache", 5, "version/size mismatch, entry discarded"));
  }
  std::string expect(reinterpret_cast<const char*>(raw.data() + 12 + len), 64);
  if (sha256hex(raw.data() + 12, len) != expect) {
    std::error_code ec;
    std::filesystem::remove(p, ec);
    corruptDiscard_ = true;
    return Result<Entry>::fail(Error::make("cache", 6, "payload hash mismatch, entry discarded"));
  }
  Entry e{key, std::vector<std::uint8_t>(raw.begin() + 12, raw.begin() + 12 + len)};
  return Result<Entry>::success(std::move(e));
}

Result<void> ShaderCache::store(const std::string& key, const std::vector<std::uint8_t>& payload) {
  if (!safeKey(key))
    return Result<void>::fail(Error::make("cache", 1, "bad cache key"));
  // Game-derived shader data stays outside the source tree (policy guard).
  if (auto g = fs::ensureOutsideRepo(dir_); !g.ok())
    return Result<void>::fail(Error::make("cache", 10, g.error().message));
  std::size_t entries = 0, bytes = 0;
  if (auto s = stats(entries, bytes); s.ok()) {
    if (entries >= maxEntries_ || bytes + payload.size() > maxBytes_)
      return Result<void>::fail(Error::make("cache", 7, "cache budget exhausted (refusing store)"));
  }
  std::error_code ec;
  std::filesystem::create_directories(dir_, ec);
  if (ec) return Result<void>::fail(Error::make("cache", 8, "cannot create cache dir"));
  std::string hex = sha256hex(payload.empty() ? nullptr : payload.data(), payload.size());
  std::vector<std::uint8_t> raw;
  raw.insert(raw.end(), {'S', 'B', 'P', 'C'});
  std::uint32_t ver = kPipelineCacheVersion, len = static_cast<std::uint32_t>(payload.size());
  for (int i = 0; i < 4; ++i) raw.push_back(static_cast<std::uint8_t>(ver >> (8 * i)));
  for (int i = 0; i < 4; ++i) raw.push_back(static_cast<std::uint8_t>(len >> (8 * i)));
  raw.insert(raw.end(), payload.begin(), payload.end());
  raw.insert(raw.end(), hex.begin(), hex.end());
  std::filesystem::path p = std::filesystem::path(dir_) / (key + ".bin");
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) return Result<void>::fail(Error::make("cache", 9, "cannot write cache entry"));
  f.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
  return f ? Result<void>::success()
           : Result<void>::fail(Error::make("cache", 9, "cannot write cache entry"));
}

Result<void> ShaderCache::clear() {
  std::error_code ec;
  std::size_t removed = 0;
  for (auto& e : std::filesystem::directory_iterator(dir_, ec)) {
    if (e.is_regular_file() && e.path().extension() == ".bin") {
      std::filesystem::remove(e.path(), ec);
      ++removed;
    }
  }
  (void)removed;
  return Result<void>::success();
}

Result<void> ShaderCache::stats(std::size_t& entries, std::size_t& bytes) {
  entries = 0;
  bytes = 0;
  std::error_code ec;
  for (auto& e : std::filesystem::directory_iterator(dir_, ec)) {
    if (e.is_regular_file() && e.path().extension() == ".bin") {
      ++entries;
      bytes += e.file_size(ec);
    }
  }
  return Result<void>::success();
}

}  // namespace cache
}  // namespace sb
