#pragma once
// Minimal AMD64 PE import census: headers + import/delay-import tables parsed
// from bytes, never loaded or executed.
//
// Idea informed by five-star-recomp PeImage.cs (DOS/COFF/optional layout,
// RVA-to-offset mapping, directory table) + PeImportReader.cs (20-byte import
// descriptors, 32-byte delay descriptors with RVA-vs-VA bias, ordinal top
// bit, hint+name thunks, bounded loops) and Xbox2Windows tools/pe_inventory.py
// + compat_report.py (AMD64-only report, deterministic rows). Independently
// implemented here in portable C++ with explicit caps; no code copied.
//
// Policy: structural problems (bad magic, unsupported machine, truncated
// headers) fail. Per-row data problems (bad RVA, missing name) skip that
// descriptor/thunk and continue, so one corrupt entry cannot kill the census.
#include <cstdint>
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {
namespace pe {

struct PeSection {
  std::string name;
  std::uint32_t rva = 0;
  std::uint32_t virtualSize = 0;
  std::uint32_t rawOffset = 0;
  std::uint32_t rawSize = 0;
  std::uint32_t characteristics = 0;  // SCN_MEM_* bits; 0 when not parsed
};

struct PeImportRow {
  std::string kind;  // "normal" | "delay"
  std::string module;
  std::string symbol;  // "#123" for ordinals
};

struct PeSummary {
  bool is64 = false;
  std::uint16_t machine = 0;
  std::uint32_t entryRva = 0;
  std::uint64_t imageBase = 0;
  std::vector<PeSection> sections;
  std::vector<PeImportRow> imports;
};

inline constexpr std::uint16_t kMachineAmd64 = 0x8664;
inline constexpr std::size_t kMaxSections = 96;
inline constexpr std::size_t kMaxDescriptors = 4096;
inline constexpr std::size_t kMaxThunks = 65536;
inline constexpr std::size_t kMaxAscii = 256;

// Parses headers + both import tables. Fails on non-PE input, unsupported
// machines, or truncated headers; skips corrupt rows individually.
Result<PeSummary> parseImports(const std::uint8_t* data, std::size_t len);

}  // namespace pe
}  // namespace sb
