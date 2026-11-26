#pragma once
// FNV-1a (matches the web prototype's fingerprint hashes) and SHA-256
// (independent implementation, verified against the "abc" vector in tests).
#include <cstdint>
#include <string>
#include <vector>

namespace sb {

inline std::uint32_t fnv1a32(const void* data, std::size_t len) {
  const auto* p = static_cast<const unsigned char*>(data);
  std::uint32_t h = 0x811c9dc5u;
  for (std::size_t i = 0; i < len; ++i) {
    h ^= p[i];
    h *= 0x01000193u;
  }
  return h;
}
inline std::uint32_t fnv1a32(const std::string& s) { return fnv1a32(s.data(), s.size()); }

std::string fnv1a32hex(const void* data, std::size_t len);
std::string sha256hex(const void* data, std::size_t len);
inline std::string sha256hex(const std::string& s) { return sha256hex(s.data(), s.size()); }
inline std::string sha256hex(const std::vector<std::uint8_t>& v) {
  return v.empty() ? sha256hex(nullptr, 0) : sha256hex(v.data(), v.size());
}

}  // namespace sb
