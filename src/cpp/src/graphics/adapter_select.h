#pragma once
// Portable GPU-adapter choice: auto/warp/index/name selector with
// digit-boundary numeric model tokens, over hardware-first ordering.
//
// Idea informed by XCL `crates/xcl/src/gpu/adapter.rs` (+ `adapter_tests.rs`)
// at `b49be1836844` (Selector::parse/allows, contains_numeric_token,
// select_indices: auto/empty picks hardware in order, "warp" the software
// renderer, a plain number the N-th hardware adapter with an out-of-range
// number falling back to a standalone numeric model token, anything else a
// case-insensitive name part) and by AnyXboxX
// runtime/gpu_runner/adapter.cpp (env filter, hardware-only order, name
// match, WARP fallback, no-match report listing adapters). Independently
// implemented here over our own AdapterInfo with no DXGI dependency so it is
// unit-testable anywhere.
#include <cstddef>
#include <string>
#include <vector>
#include "graphics/abstraction.h"

namespace sb {
namespace gfx {

// Which adapter a filter string means: every hardware adapter in order,
// every software adapter in order, the N-th hardware adapter (0-based), or
// the adapters whose description contains a name part (case-insensitive).
enum class AdapterSelector { Auto, Warp, Index, Name };

struct AdapterSelection {
  AdapterSelector kind = AdapterSelector::Auto;
  std::size_t index = 0;  // only for Index
  std::string name;       // only for Name, already lower-cased + trimmed
};

// "" or "auto" (trimmed, case-insensitive) -> Auto; "warp" -> Warp; a plain
// digit string ("0", but not "007") -> Index (oversized values that do not
// fit fall back to Name, like a failed usize parse); anything else ->
// Name(lower-cased). Never throws.
AdapterSelection parseAdapterSelector(const std::string& filter);

// True when `digits` (non-empty) occurs in `haystack` not immediately
// preceded or followed by another ASCII digit: "5070" matches "RTX 5070"
// but "2", "208" and "80" do not match "RTX 2080". Never throws.
bool containsNumericToken(const std::string& haystack, const std::string& digits);

// Every matching index in adapter order. Auto: all hardware. Warp: all
// software. Index(n): the n-th hardware adapter, or — when n is out of
// range — the hardware adapters carrying n as a numeric model token. Name:
// every adapter (hardware or software) whose description contains the part.
// Never throws.
std::vector<int> selectAdapterIndices(const std::vector<AdapterInfo>& adapters,
                                      const AdapterSelection& sel);

// One-line meaning of a parsed selector for the settings UI, e.g.
// "auto: first hardware adapter in high-performance order". Names the rule
// only — resolving it against real adapters happens at device creation
// with honest capability skips. Never throws.
std::string describeSelector(const AdapterSelection& sel);

// Index into `adapters`, or -1 when nothing fits: the first index from
// selectAdapterIndices, preserving the historical rules for the rest —
// Auto with no hardware adapter returns index 0, and a miss falls back to
// the first software adapter when fallbackWarp is set.
int chooseAdapter(const std::vector<AdapterInfo>& adapters, const std::string& filter,
                  bool fallbackWarp = true);
bool isWarpRequest(const std::string& filter);

}  // namespace gfx
}  // namespace sb
