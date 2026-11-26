#pragma once
// PE export directory census from bytes: names, ordinals, RVAs, forwarders.
// Never loads or executes the image.
//
// Idea informed by victory-fork victory-pe/src/image.rs parse_exports
// (Address(0) gaps skipped, ForwardByName/ForwardByOrdinal "dll.name" /
// "dll.#ord" display, rva_to_file_offset overflow guard) and XCL
// resolve_export_of_mapped (rsplit on LAST '.' for dotted DLLs, depth cap 8,
// GetProcAddress-null on missing). Independently implemented here in portable
// C++ with explicit caps; no code copied.
//
// Policy: structural problems (bad magic, unsupported machine, truncated
// headers, absurd table sizes) fail. Per-row problems (bad RVA, missing
// name) skip that row and continue.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "core/result.h"
#include "formats/pe_imports.h"  // PeSection reuse (same layout semantics)

namespace sb {
namespace pe {

struct ExportRow {
  std::optional<std::string> name;  // empty for ordinal-only exports
  std::uint16_t ordinal = 0;
  std::optional<std::uint32_t> rva;          // absent for forwarders
  std::optional<std::string> forwarder;      // "dll.symbol" / "dll.#ord"
};

struct ImageFeatures {
  std::uint16_t machine = 0;
  bool hasExports = false;
  bool hasTls = false;
  bool hasLoadConfig = false;
  bool hasException = false;
  std::uint32_t exceptionEntries = 0;
  std::uint32_t exceptionTrailing = 0;
};

inline constexpr std::size_t kMaxExports = 65536;

struct ExportSummary {
  ImageFeatures features;
  std::vector<PeSection> sections;
  std::vector<ExportRow> exports;
};

// Parses headers + sections + the export directory (index 0). Fails on
// non-PE input, unsupported machines, truncated headers, or absurd table
// sizes; skips corrupt rows individually.
Result<ExportSummary> parseExports(const std::uint8_t* data, std::size_t len);

// Sort key matching Xbox2Windows compat_report._row_key/_symbol_key spirit:
// ordinal-only rows first (by ordinal), then names casefolded. Deterministic
// across runs; independent implementation.
bool exportLess(const ExportRow& a, const ExportRow& b);

}  // namespace pe
}  // namespace sb
