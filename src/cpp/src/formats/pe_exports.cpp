#include "formats/pe_exports.h"

#include <cctype>

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
  return false;
}

std::string lowerStr(const std::string& s) {
  std::string o = s;
  for (char& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return o;
}

}  // namespace

bool exportLess(const ExportRow& a, const ExportRow& b) {
  // Ordinal-only rows sort before named rows (compat_report._symbol_key:
  // ordinals are (0, n), names are (1, ...)).
  bool aOrd = !a.name.has_value();
  bool bOrd = !b.name.has_value();
  if (aOrd && bOrd) return a.ordinal < b.ordinal;
  if (aOrd) return true;
  if (bOrd) return false;
  std::string al = lowerStr(*a.name), bl = lowerStr(*b.name);
  if (al != bl) return al < bl;
  if (a.name != b.name) return *a.name < *b.name;
  return a.ordinal < b.ordinal;
}

Result<ExportSummary> parseExports(const std::uint8_t* data, std::size_t len) {
  if (!data && len != 0)
    return Result<ExportSummary>::fail(Error::make("pe", 1, "null image data"));
  if (len < 64) return Result<ExportSummary>::fail(Error::make("pe", 1, "too small for DOS header"));
  if (!(data[0] == 'M' && data[1] == 'Z'))
    return Result<ExportSummary>::fail(Error::make("pe", 2, "missing MZ header"));
  std::uint32_t e_lfanew = 0;
  if (!readU32(data, len, 0x3C, e_lfanew))
    return Result<ExportSummary>::fail(Error::make("pe", 3, "truncated e_lfanew"));
  if (e_lfanew > len || len - e_lfanew < 6)
    return Result<ExportSummary>::fail(Error::make("pe", 3, "bad e_lfanew"));
  if (!(data[e_lfanew] == 'P' && data[e_lfanew + 1] == 'E' && data[e_lfanew + 2] == 0 &&
        data[e_lfanew + 3] == 0))
    return Result<ExportSummary>::fail(Error::make("pe", 4, "no PE signature"));

  const std::size_t coff = e_lfanew + 4;
  std::uint16_t machine = 0, numSections = 0, sizeOptional = 0;
  if (!readU16(data, len, coff, machine) || !readU16(data, len, coff + 2, numSections) ||
      !readU16(data, len, coff + 16, sizeOptional))
    return Result<ExportSummary>::fail(Error::make("pe", 5, "truncated COFF header"));
  if (machine != kMachineAmd64)
    return Result<ExportSummary>::fail(
        Error::make("pe", 6, "unsupported machine (expected AMD64)"));
  if (numSections > kMaxSections)
    return Result<ExportSummary>::fail(Error::make("pe", 7, "section count over cap"));

  const std::size_t opt = coff + 20;
  if (sizeOptional < 112 || opt + sizeOptional > len)
    return Result<ExportSummary>::fail(Error::make("pe", 8, "truncated optional header"));
  std::uint16_t magic = 0;
  if (!readU16(data, len, opt, magic) || magic != 0x20B)
    return Result<ExportSummary>::fail(Error::make("pe", 9, "not PE32+ (64-bit required)"));

  std::uint32_t dirCount = 0;
  if (!readU32(data, len, opt + 108, dirCount))
    return Result<ExportSummary>::fail(Error::make("pe", 8, "truncated directory count"));
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
  std::vector<PeSection> secs;
  for (std::uint16_t i = 0; i < numSections; ++i) {
    const std::size_t at = secTab + i * 40;
    if (at + 40 > len)
      return Result<ExportSummary>::fail(Error::make("pe", 10, "truncated section table"));
    PeSection s;
    std::size_t nameLen = 0;
    while (nameLen < 8 && data[at + nameLen] != 0) ++nameLen;
    s.name.assign(reinterpret_cast<const char*>(data + at), nameLen);
    readU32(data, len, at + 8, s.virtualSize);
    readU32(data, len, at + 12, s.rva);
    readU32(data, len, at + 16, s.rawSize);
    readU32(data, len, at + 20, s.rawOffset);
    readU32(data, len, at + 36, s.characteristics);
    secs.push_back(std::move(s));
  }

  ExportSummary out;
  out.sections = secs;
  out.features.machine = machine;
  std::uint32_t rva = 0, size = 0;
  dir(0, rva, size);
  out.features.hasExports = (rva != 0 && size != 0);
  dir(3, rva, size);
  out.features.hasException = (rva != 0 && size != 0);
  if (out.features.hasException && machine == kMachineAmd64) {
    out.features.exceptionEntries = size / 12;
    out.features.exceptionTrailing = size % 12;
  }
  dir(9, rva, size);
  out.features.hasTls = (rva != 0 && size != 0);
  dir(10, rva, size);
  out.features.hasLoadConfig = (rva != 0 && size != 0);

  std::uint32_t expRva = 0, expSize = 0;
  dir(0, expRva, expSize);
  if (expRva == 0 || expSize == 0) return Result<ExportSummary>::ok(std::move(out));

  std::size_t expOff = 0;
  if (!rvaToOffset(secs, expRva, len, expOff))
    return Result<ExportSummary>::fail(Error::make("pe", 11, "export directory outside image"));
  if (expOff + 40 > len)
    return Result<ExportSummary>::fail(Error::make("pe", 11, "truncated export directory"));
  std::uint32_t base = 0, numFuncs = 0, numNames = 0, addrFuncs = 0, addrNames = 0,
                addrOrdinals = 0;
  readU32(data, len, expOff + 16, base);
  readU32(data, len, expOff + 20, numFuncs);
  readU32(data, len, expOff + 24, numNames);
  readU32(data, len, expOff + 28, addrFuncs);
  readU32(data, len, expOff + 32, addrNames);
  readU32(data, len, expOff + 36, addrOrdinals);
  if (numFuncs > kMaxExports || numNames > kMaxExports)
    return Result<ExportSummary>::fail(Error::make("pe", 12, "export table over cap"));
  if (numFuncs == 0) return Result<ExportSummary>::ok(std::move(out));
  if (numNames > numFuncs)
    return Result<ExportSummary>::fail(Error::make("pe", 12, "more names than functions"));

  auto inExportRange = [&](std::uint32_t v) {
    return v >= expRva && static_cast<std::uint64_t>(v) - expRva < expSize;
  };

  // Function RVAs.
  std::vector<std::uint32_t> funcs;
  funcs.reserve(numFuncs);
  for (std::uint32_t i = 0; i < numFuncs; ++i) {
    std::uint64_t at = static_cast<std::uint64_t>(addrFuncs) + i * 4;
    if (at > 0xFFFFFFFFULL) break;
    std::size_t off = 0;
    std::uint32_t v = 0;
    if (!rvaToOffset(secs, static_cast<std::uint32_t>(at), len, off) ||
        !readU32(data, len, off, v))
      break;
    funcs.push_back(v);
  }
  if (funcs.size() != numFuncs) return Result<ExportSummary>::ok(std::move(out));

  std::vector<bool> named(funcs.size(), false);
  // Named exports.
  for (std::uint32_t i = 0; i < numNames; ++i) {
    std::uint64_t namePtrAt = static_cast<std::uint64_t>(addrNames) + i * 4;
    std::uint64_t ordAt = static_cast<std::uint64_t>(addrOrdinals) + i * 2;
    if (namePtrAt > 0xFFFFFFFFULL || ordAt > 0xFFFFFFFFULL) continue;
    std::size_t namePtrOff = 0, ordOff = 0;
    std::uint32_t nameRva = 0;
    std::uint16_t ordIdx = 0;
    if (!rvaToOffset(secs, static_cast<std::uint32_t>(namePtrAt), len, namePtrOff) ||
        !readU32(data, len, namePtrOff, nameRva))
      continue;
    if (!rvaToOffset(secs, static_cast<std::uint32_t>(ordAt), len, ordOff) ||
        !readU16(data, len, ordOff, ordIdx))
      continue;
    if (ordIdx >= funcs.size()) continue;
    std::string sym;
    if (!asciiAt(data, len, secs, nameRva, sym)) continue;
    named[ordIdx] = true;
    std::uint32_t funcRva = funcs[ordIdx];
    ExportRow row;
    row.name = sym;
    row.ordinal = static_cast<std::uint16_t>(base + ordIdx);
    if (funcRva == 0) continue;  // unused slot despite a name: skip
    if (inExportRange(funcRva)) {
      std::string fwd;
      if (!asciiAt(data, len, secs, funcRva, fwd)) continue;
      row.forwarder = fwd;
    } else {
      row.rva = funcRva;
    }
    out.exports.push_back(std::move(row));
  }
  // Ordinal-only exports (no name entry, nonzero, not a forwarder hole).
  for (std::size_t i = 0; i < funcs.size(); ++i) {
    if (named[i]) continue;
    std::uint32_t funcRva = funcs[i];
    if (funcRva == 0) continue;  // gap in the ordinal range
    ExportRow row;
    row.ordinal = static_cast<std::uint16_t>(base + i);
    if (inExportRange(funcRva)) {
      std::string fwd;
      if (!asciiAt(data, len, secs, funcRva, fwd)) continue;
      row.forwarder = fwd;
    } else {
      row.rva = funcRva;
    }
    out.exports.push_back(std::move(row));
  }
  return Result<ExportSummary>::ok(std::move(out));
}

}  // namespace pe
}  // namespace sb
