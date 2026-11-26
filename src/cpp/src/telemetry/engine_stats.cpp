#include "telemetry/engine_stats.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace sb {
namespace diag {
namespace {

bool startsWith(const std::string& s, const char* word, std::size_t n) {
  return s.size() >= n && s.compare(0, n, word) == 0;
}

// Strict pid token: one or more ASCII digits, value fits in 32 bits.
bool parsePidToken(const std::string& token, std::uint32_t& pid) {
  if (token.empty() || token.size() > 10) return false;
  std::uint64_t acc = 0;
  for (char c : token) {
    if (c < '0' || c > '9') return false;
    acc = acc * 10 + static_cast<std::uint64_t>(c - '0');
    if (acc > std::numeric_limits<std::uint32_t>::max()) return false;
  }
  pid = static_cast<std::uint32_t>(acc);
  return true;
}

}  // namespace

std::optional<std::pair<std::uint32_t, std::string>> parseEngineInstance(
    const std::string& name) {
  constexpr char kPrefix[] = "pid_";
  constexpr std::size_t kPrefixLen = 4;
  constexpr char kKind[] = "engtype_";
  constexpr std::size_t kKindLen = 8;
  if (!startsWith(name, kPrefix, kPrefixLen)) return std::nullopt;
  const std::string rest = name.substr(kPrefixLen);
  const std::size_t cut = rest.find('_');
  const std::string token =
      cut == std::string::npos ? rest : rest.substr(0, cut);
  std::uint32_t pid = 0;
  if (!parsePidToken(token, pid)) return std::nullopt;
  const std::size_t at = name.find(kKind);
  if (at == std::string::npos) return std::nullopt;
  std::string kind = name.substr(at + kKindLen);
  const std::size_t again = kind.find(kKind);
  if (again != std::string::npos) kind.erase(again);
  return std::make_pair(pid, kind);
}

BusiestEngines busiestEngineKind(const std::vector<EngineSample>& engines,
                                 std::optional<std::uint32_t> pid) {
  BusiestEngines out;
  try {
    for (const EngineSample& e : engines) {
      if (pid.has_value() && e.pid != *pid) continue;
      out.byKind[e.kind] += e.percent;
    }
    double top = 0.0;
    for (const auto& kv : out.byKind) {
      if (kv.second > top) top = kv.second;
    }
    if (!(top >= 0.0) || !(top <= 100.0)) {
      if (!(top >= 0.0)) {
        top = 0.0;  // NaN guard: no finite sum ever won
      } else {
        top = 100.0;  // sums over 100 (several engines of a kind) show as 100
      }
    }
    out.top = static_cast<float>(top);
  } catch (...) {
    out.byKind.clear();
    out.top = 0.0f;
  }
  return out;
}

}  // namespace diag
}  // namespace sb
