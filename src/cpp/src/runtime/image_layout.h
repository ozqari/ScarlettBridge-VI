#pragma once
// Portable image-layout validation for mapped PE images: containment,
// executable/writable section tests, section protections, relocations, TLS
// callback bounds. No mapping, no OS calls — pure checks over headers.
//
// Idea informed by XCL loader.rs (reserve+commit, section_protection
// mapping, DIR64/HIGHLOW relocs with unsupported-type warning, TLS install
// bounds, page-protection pass) + ReCity pe_loader.cpp require_range /
// read_string / rva_in (every image RVA checked first) + Xbox2Windows
// pe_map.cpp protection()/executable()/writable_section()/seed_security_cookie
// bounds + victory-pe va_to_rva + MAX_TLS_CALLBACKS cap. Independently
// implemented here; no code copied, no Windows headers.
#include <cstdint>
#include <string>
#include <vector>
#include "formats/pe_imports.h"  // PeSection reuse

namespace sb {
namespace layout {

enum class Protection {
  NoAccess,
  ReadOnly,
  ReadWrite,
  Execute,
  ExecuteRead,
  ExecuteReadWrite,
};

std::string toString(Protection p);

// Characteristics use the WinNT SCN_MEM_* bits (also read by pe_map.cpp):
// EXECUTE 0x20000000, READ 0x40000000, WRITE 0x80000000.
Protection sectionProtection(std::uint32_t characteristics);

// Overflow-safe [rva, rva+len) containment in an image of imageSize bytes
// (ReCity require_range spirit: truncated images fail instead of over-reading).
bool rvaInsideImage(std::uint64_t rva, std::uint64_t len, std::uint32_t imageSize);

// Mapped span of a section: VirtualSize when nonzero else raw size
// (pe_map executable()/writable_section() use the same span).
std::uint64_t sectionSpan(const PeSection& s);

bool isExecutableRva(std::uint32_t rva, const std::vector<PeSection>& sections);
bool isWritableRva(std::uint32_t rva, std::uint32_t size,
                   const std::vector<PeSection>& sections);
// Entry point must lie inside the image AND in an executable section.
bool isValidEntryPoint(std::uint32_t entryRva, std::uint32_t imageSize,
                       const std::vector<PeSection>& sections);

inline constexpr std::uint16_t kRelocHighLow = 3;
inline constexpr std::uint16_t kRelocDir64 = 10;

bool isSupportedReloc(std::uint16_t type);
// Apply one relocation: DIR64 adds the full delta, HIGHLOW adds it mod 2^32.
// Returns false for unsupported types (caller warns, like XCL's fallback).
bool applyReloc(std::uint64_t value, std::uint16_t type, std::int64_t delta,
                std::uint64_t& out);

inline constexpr std::size_t kMaxTlsCallbacks = 1024;

// Every TLS callback RVA must lie inside the image; at most 1024 are walked
// (victory-pe MAX_TLS_CALLBACKS). Empty is valid.
bool validateTlsCallbacks(const std::vector<std::uint32_t>& callbackRvas,
                          std::uint32_t imageSize);

}  // namespace layout
}  // namespace sb
