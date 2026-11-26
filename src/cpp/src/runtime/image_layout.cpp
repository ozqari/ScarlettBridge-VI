#include "runtime/image_layout.h"

namespace sb {
namespace layout {
namespace {

inline constexpr std::uint32_t kScnExecute = 0x20000000u;
inline constexpr std::uint32_t kScnRead = 0x40000000u;
inline constexpr std::uint32_t kScnWrite = 0x80000000u;

}  // namespace

std::string toString(Protection p) {
  switch (p) {
    case Protection::NoAccess: return "noaccess";
    case Protection::ReadOnly: return "readonly";
    case Protection::ReadWrite: return "readwrite";
    case Protection::Execute: return "execute";
    case Protection::ExecuteRead: return "execute-read";
    case Protection::ExecuteReadWrite: return "execute-readwrite";
  }
  return "?";
}

Protection sectionProtection(std::uint32_t characteristics) {
  bool x = (characteristics & kScnExecute) != 0;
  bool r = (characteristics & kScnRead) != 0;
  bool w = (characteristics & kScnWrite) != 0;
  if (x && w) return Protection::ExecuteReadWrite;
  if (x && r) return Protection::ExecuteRead;
  if (x) return Protection::Execute;
  if (w) return Protection::ReadWrite;
  if (r) return Protection::ReadOnly;
  return Protection::NoAccess;
}

bool rvaInsideImage(std::uint64_t rva, std::uint64_t len, std::uint32_t imageSize) {
  if (rva > imageSize) return false;
  return len <= static_cast<std::uint64_t>(imageSize) - rva;
}

std::uint64_t sectionSpan(const PeSection& s) {
  return s.virtualSize != 0 ? s.virtualSize : s.rawSize;
}

bool isExecutableRva(std::uint32_t rva, const std::vector<PeSection>& sections) {
  for (const auto& s : sections) {
    std::uint64_t span = sectionSpan(s);
    if (span == 0) continue;
    if (rva >= s.rva && static_cast<std::uint64_t>(rva) - s.rva < span) {
      // Characteristic bit, not name: matches pe_map executable().
      return (s.characteristics & kScnExecute) != 0;
    }
  }
  return false;
}

bool isWritableRva(std::uint32_t rva, std::uint32_t size,
                   const std::vector<PeSection>& sections) {
  for (const auto& s : sections) {
    std::uint64_t span = sectionSpan(s);
    if (span == 0) continue;
    if (rva >= s.rva && static_cast<std::uint64_t>(rva) - s.rva <= span) {
      if (size > span - (rva - s.rva)) return false;
      return (s.characteristics & kScnWrite) != 0;
    }
  }
  return false;
}

bool isValidEntryPoint(std::uint32_t entryRva, std::uint32_t imageSize,
                       const std::vector<PeSection>& sections) {
  if (entryRva == 0) return false;
  if (!rvaInsideImage(entryRva, 1, imageSize)) return false;
  return isExecutableRva(entryRva, sections);
}

bool isSupportedReloc(std::uint16_t type) {
  return type == kRelocHighLow || type == kRelocDir64;
}

bool applyReloc(std::uint64_t value, std::uint16_t type, std::int64_t delta,
                std::uint64_t& out) {
  if (type == kRelocDir64) {
    out = static_cast<std::uint64_t>(static_cast<std::int64_t>(value) + delta);
    return true;
  }
  if (type == kRelocHighLow) {
    std::uint32_t lo = static_cast<std::uint32_t>(value);
    lo = static_cast<std::uint32_t>(static_cast<std::int64_t>(lo) + delta);
    out = lo;
    return true;
  }
  return false;
}

bool validateTlsCallbacks(const std::vector<std::uint32_t>& callbackRvas,
                          std::uint32_t imageSize) {
  if (callbackRvas.size() > kMaxTlsCallbacks) return false;
  for (std::uint32_t rva : callbackRvas) {
    if (!rvaInsideImage(rva, 1, imageSize)) return false;
  }
  return true;
}

}  // namespace layout
}  // namespace sb
