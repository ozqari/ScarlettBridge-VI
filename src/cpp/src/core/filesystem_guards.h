#pragma once
// Guards that keep game-derived data outside the repository.
//
// Rule (idea adopted from victory-fork crates/victory-core/src/policy.rs,
// independently implemented): anything produced from user files — converted
// assets, shader caches, probe reports, crash dumps — is as much "from the
// game" as the files themselves, so tools that write such output must call
// ensureOutsideRepo() on the destination first. Source-built fixtures and
// code are the only things that belong in-tree.
//
// Detection is by marker file: a directory tree containing
// SB-ROOT-MARKER.txt is a source tree. The check canonicalizes the longest
// existing prefix of the path and re-appends the rest, so not-yet-existing
// destinations are still compared correctly.
#include <string>
#include "core/result.h"

namespace sb {
namespace fs {

inline constexpr const char* kTreeMarker = "SB-ROOT-MARKER.txt";

// Returns ok only if `path` is outside every source tree (no ancestor
// directory, after canonicalization, contains kTreeMarker, and the path is
// not below the compiled tree root when it can be determined).
Result<void> ensureOutsideRepo(const std::string& path);

}  // namespace fs
}  // namespace sb
