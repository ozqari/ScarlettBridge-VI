#pragma once
// Forwarded-export chain resolution ("dll.symbol" / "dll.#ord").
// Never loads anything: the caller supplies an export lookup.
//
// Idea informed by XCL resolver.rs resolve_export_of_mapped[_inner]
// (ExportTarget::Forwarder split on the LAST '.', "#n" ordinals, depth > 8
// cutoff, GetProcAddress-null on unresolvable forwarders, canonical ".dll"
// suffixing). Independently implemented here in portable C++; no code copied.
#include <cstdint>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace sb {

struct ForwarderTarget {
  std::string dllKey;  // lower-cased, always ends in ".dll"
  bool isOrdinal = false;
  std::uint16_t ordinal = 0;
  std::string name;  // when !isOrdinal
};

// Split "Dll.Name" on the LAST '.' (dotted DLLs stay intact), "#n" suffix
// means ordinal. Returns nullopt when there is no dot or a side is empty.
std::optional<ForwarderTarget> parseForwarderSpec(const std::string& spec);

struct ExportOutcome {
  bool isForwarder = false;
  std::string forwarder;   // valid when isForwarder
  std::uint32_t rva = 0;   // valid when !isForwarder
};

// Lookup of one export: dllKey is lower-cased with ".dll" suffix, symbol is
// "#n" for ordinals else the exact name. Returns nullopt when the export
// does not exist (missing stays missing: no stub is synthesized).
using ExportLookup =
    std::function<std::optional<ExportOutcome>(const std::string&, const std::string&)>;

struct ForwardResolveResult {
  std::string dllKey;
  std::uint32_t rva = 0;
};

inline constexpr int kMaxForwarderDepth = 8;

// Follow a forwarder chain (the initial spec plus nested forwarders) up to
// kMaxForwarderDepth hops. Returns nullopt on any missing export, bad spec,
// or depth overflow — mirroring GetProcAddress returning NULL on
// unresolvable forwarders while static imports would stub.
std::optional<ForwardResolveResult> resolveForwarderChain(const std::string& spec,
                                                        const ExportLookup& lookup);

}  // namespace sb
