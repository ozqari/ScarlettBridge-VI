#include "formats/pe_imports.h"

namespace sb {
namespace pe {
namespace {

bool readU16(const std::uint8_t* d, std::size_t n, std::size_t at, std::uint16_t& out) {
  if (at > n || n - at < 2) return false;
  out = static_cast<std::uint16_t>(d[at]) | (static_cast<std::uint16_t>(d[at + 1]) << 8);
  return true;
}
bool readU32(const std::uint8_t* d, std::size_t n, std::size_t at, std::uint32_t& out) {
  if (at > n || n - at < 4) return false;
  out = static_cast<std::uint32_t>(d[at]) | (static_cast<std::uint32_t>(d[at + 1]) << 8) |
        (static_cast<std::uint32_t>(d[at + 2]) << 16) |
        (static_cast<std::uint32_t>(d[at + 3]) << 24);
  return true;
}
bool readU64(const std::uint8_t* d, std::size_t n, std::size_t at, std::uint64_t& out) {
  if (at > n || n - at < 8) return false;
  out = 0;
  for (int i = 0; i < 8; ++i) out |= static_cast<std::uint64_t>(d[at + i]) << (8 * i);
  return true;
}

// File offset of an RVA: the section whose [rva, rva+max(vsize,raw)) holds it,
// else the headers when the RVA points below the first section.
bool rvaToOffset(const std::vector<PeSection>& secs, std::uint32_t rva, std::size_t fileLen,
                 std::size_t& out) {
  for (const auto& s : secs) {
    std::uint64_t span = s.virtualSize > s.rawSize ? s.virtualSize : s.rawSize;
    if (rva >= s.rva && static_cast<std::uint64_t>(rva) - s.rva < span) {
      std::uint64_t off = static_cast<std::uint64_t>(s.rawOffset) + (rva - s.rva);
      if (off >= fileLen) return false;
      out = static_cast<std::size_t>(off);
      return true;
    }
  }
  if (secs.empty() || rva < secs.front().rva) {
    if (rva >= fileLen) return false;
    out = rva;
    return true;
  }
  return false;
}

bool asciiAt(const std::uint8_t* d, std::size_t n, const std::vector<PeSection>& secs,
             std::uint32_t rva, std::string& out) {
  std::size_t off = 0;
  if (!rvaToOffset(secs, rva, n, off)) return false;
  out.clear();
  for (std::size_t i = 0; i < kMaxAscii; ++i) {
    if (off + i >= n) return false;
    char c = static_cast<char>(d[off + i]);
    if (c == 0) return !out.empty();
    out.push_back(c);
  }
  return false;  // no NUL within the cap: treat as invalid
}

void readThunks(const std::uint8_t* d, std::size_t n, const std::vector<PeSection>& secs,
                std::uint32_t tableRva, bool is64, const std::string& kind,
                const std::string& dll, std::uint64_t vaBias,
                std::vector<PeImportRow>& rows) {
  const std::size_t step = is64 ? 8 : 4;
  const std::uint64_t ordinalFlag = is64 ? (1ULL << 63) : (1ULL << 31);
  for (std::size_t i = 0; i < kMaxThunks; ++i) {
    std::uint64_t entry = 0;
    std::size_t off = 0;
    // Guard against wrap: tableRva + i*step must stay in u32 range.
    std::uint64_t at = static_cast<std::uint64_t>(tableRva) + i * step;
    if (at > 0xFFFFFFFFULL) break;
    if (!rvaToOffset(secs, static_cast<std::uint32_t>(at), n, off)) break;
    if (is64) {
      if (!readU64(d, n, off, entry)) break;
    } else {
      std::uint32_t e32 = 0;
      if (!readU32(d, n, off, e32)) break;
      entry = e32;
    }
    if (entry == 0) break;
    if (entry & ordinalFlag) {
      rows.push_back(PeImportRow{kind, dll, "#" + std::to_string(entry & 0xFFFF)});
      continue;
    }
    // Otherwise the thunk names a hint+name blob: an RVA, or (delay tables
    // with VA pointers) a VA that must be un-biased first.
    if (entry < vaBias) continue;
    std::uint64_t nameRva64 = entry - vaBias;
    if (nameRva64 > 0xFFFFFFFFULL || nameRva64 + 2 > 0xFFFFFFFFULL) continue;
    std::uint32_t nameRva = static_cast<std::uint32_t>(nameRva64);
    std::size_t hintOff = 0;
    if (!rvaToOffset(secs, nameRva, n, hintOff)) continue;
    // Hint must exist (2 bytes); the name follows it.
    std::uint16_t hint = 0;
    if (!readU16(d, n, hintOff, hint)) continue;
    (void)hint;
    std::string sym;
    if (!asciiAt(d, n, secs, nameRva + 2, sym)) continue;
    rows.push_back(PeImportRow{kind, dll, sym});
  }
}

}  // namespace

Result<PeSummary> parseImports(const std::uint8_t* data, std::size_t len) {
  if (!data && len != 0)
    return Result<PeSummary>::fail(Error::make("pe", 1, "null image data"));
  if (len < 64) return Result<PeSummary>::fail(Error::make("pe", 1, "too small for DOS header"));
  if (!(data[0] == 'M' && data[1] == 'Z'))
    return Result<PeSummary>::fail(Error::make("pe", 2, "missing MZ header"));
  std::uint32_t e_lfanew = 0;
  if (!readU32(data, len, 0x3C, e_lfanew))
    return Result<PeSummary>::fail(Error::make("pe", 3, "truncated e_lfanew"));
  if (e_lfanew > len || len - e_lfanew < 6)
    return Result<PeSummary>::fail(Error::make("pe", 3, "bad e_lfanew"));
  if (!(data[e_lfanew] == 'P' && data[e_lfanew + 1] == 'E' && data[e_lfanew + 2] == 0 &&
        data[e_lfanew + 3] == 0))
    return Result<PeSummary>::fail(Error::make("pe", 4, "no PE signature"));

  const std::size_t coff = e_lfanew + 4;
  std::uint16_t machine = 0, numSections = 0, sizeOptional = 0;
  if (!readU16(data, len, coff, machine) || !readU16(data, len, coff + 2, numSections) ||
      !readU16(data, len, coff + 16, sizeOptional))
    return Result<PeSummary>::fail(Error::make("pe", 5, "truncated COFF header"));
  if (machine != kMachineAmd64)
    return Result<PeSummary>::fail(
        Error::make("pe", 6, "unsupported machine (expected AMD64)"));
  if (numSections > kMaxSections)
    return Result<PeSummary>::fail(Error::make("pe", 7, "section count over cap"));

  const std::size_t opt = coff + 20;
  if (sizeOptional < 112 || opt + sizeOptional > len)
    return Result<PeSummary>::fail(Error::make("pe", 8, "truncated optional header"));
  std::uint16_t magic = 0;
  if (!readU16(data, len, opt, magic) || magic != 0x20B)
    return Result<PeSummary>::fail(Error::make("pe", 9, "not PE32+ (64-bit required)"));

  PeSummary out;
  out.is64 = true;
  out.machine = machine;
  if (!readU32(data, len, opt + 16, out.entryRva))
    return Result<PeSummary>::fail(Error::make("pe", 8, "truncated entry point"));
  if (!readU64(data, len, opt + 24, out.imageBase))
    return Result<PeSummary>::fail(Error::make("pe", 8, "truncated image base"));

  std::uint32_t dirCount = 0;
  if (!readU32(data, len, opt + 108, dirCount))
    return Result<PeSummary>::fail(Error::make("pe", 8, "truncated directory count"));
  if (dirCount > 32) dirCount = 32;
  const std::size_t dirs = opt + 112;
  auto dir = [&](std::uint32_t idx, std::uint32_t& rva, std::uint32_t& size) {
    rva = 0;
    size = 0;
    if (idx >= dirCount) return;
    readU32(data, len, dirs + idx * 8, rva);
    readU32(data, len, dirs + idx * 8 + 4, size);
  };

  const std::size_t secTab = opt + sizeOptional;
  for (std::uint16_t i = 0; i < numSections; ++i) {
    const std::size_t at = secTab + i * 40;
    if (at + 40 > len)
      return Result<PeSummary>::fail(Error::make("pe", 10, "truncated section table"));
    PeSection s;
    std::size_t nameLen = 0;
    while (nameLen < 8 && data[at + nameLen] != 0) ++nameLen;
    s.name.assign(reinterpret_cast<const char*>(data + at), nameLen);
    readU32(data, len, at + 8, s.virtualSize);
    readU32(data, len, at + 12, s.rva);
    readU32(data, len, at + 16, s.rawSize);
    readU32(data, len, at + 20, s.rawOffset);
    readU32(data, len, at + 36, s.characteristics);
    out.sections.push_back(std::move(s));
  }

  // Normal imports (directory 1): 20-byte descriptors.
  {
    std::uint32_t rva = 0, size = 0;
    dir(1, rva, size);
    if (rva != 0) {
      for (std::size_t i = 0; i < kMaxDescriptors; ++i) {
        std::uint64_t at = static_cast<std::uint64_t>(rva) + i * 20;
        if (at > 0xFFFFFFFFULL) break;
        std::size_t off = 0;
        if (!rvaToOffset(out.sections, static_cast<std::uint32_t>(at), len, off)) break;
        std::uint32_t lookup = 0, name = 0, addr = 0;
        if (!readU32(data, len, off, lookup) || !readU32(data, len, off + 12, name) ||
            !readU32(data, len, off + 16, addr))
          break;
        if (lookup == 0 && name == 0 && addr == 0) break;
        std::uint32_t table = lookup != 0 ? lookup : addr;
        std::string dll;
        if (name == 0 || !asciiAt(data, len, out.sections, name, dll)) continue;
        if (table == 0) continue;
        readThunks(data, len, out.sections, table, true, "normal", dll, 0, out.imports);
      }
    }
  }
  // Delay imports (directory 13): 32-byte descriptors, RVA-vs-VA bias.
  {
    std::uint32_t rva = 0, size = 0;
    dir(13, rva, size);
    if (rva != 0) {
      for (std::size_t i = 0; i < kMaxDescriptors; ++i) {
        std::uint64_t at = static_cast<std::uint64_t>(rva) + i * 32;
        if (at > 0xFFFFFFFFULL) break;
        std::size_t off = 0;
        if (!rvaToOffset(out.sections, static_cast<std::uint32_t>(at), len, off)) break;
        std::uint32_t attrs = 0, name = 0, addr = 0, names = 0;
        if (!readU32(data, len, off, attrs) || !readU32(data, len, off + 4, name) ||
            !readU32(data, len, off + 12, addr) || !readU32(data, len, off + 16, names))
          break;
        if (name == 0 && addr == 0 && names == 0) break;
        const std::uint64_t bias = (attrs & 1) ? 0 : out.imageBase;
        auto toRva = [&](std::uint32_t va, std::uint32_t& outRva) {
          if (static_cast<std::uint64_t>(va) < bias) return false;
          std::uint64_t r = static_cast<std::uint64_t>(va) - bias;
          if (r > 0xFFFFFFFFULL) return false;
          outRva = static_cast<std::uint32_t>(r);
          return true;
        };
        std::uint32_t nameRva = 0, tableRva = 0;
        std::uint32_t tableVa = names != 0 ? names : addr;
        if (!toRva(name, nameRva) || !toRva(tableVa, tableRva)) continue;
        std::string dll;
        if (!asciiAt(data, len, out.sections, nameRva, dll)) continue;
        readThunks(data, len, out.sections, tableRva, true, "delay", dll, bias,
                   out.imports);
      }
    }
  }
  return Result<PeSummary>::success(std::move(out));
}

}  // namespace pe
}  // namespace sb
