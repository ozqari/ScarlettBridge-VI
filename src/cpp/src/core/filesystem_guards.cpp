#include "core/filesystem_guards.h"

#include <filesystem>
#include <vector>

namespace sb {
namespace fs {
namespace {

namespace fsys = std::filesystem;

// Canonicalize the longest existing prefix, re-append the missing tail, and
// normalize "." / ".." lexically (only the non-existing tail needs it).
fsys::path resolveBestEffort(const fsys::path& p, std::error_code& ec) {
  fsys::path abs = p.is_absolute() ? p : fsys::current_path(ec) / p;
  if (ec) return abs;
  fsys::path existing = abs;
  std::vector<fsys::path> tail;
  while (!existing.empty()) {
    std::error_code cec;
    fsys::path canon = fsys::canonical(existing, cec);
    if (!cec) {
      for (auto it = tail.rbegin(); it != tail.rend(); ++it) canon /= *it;
      return canon.lexically_normal();
    }
    if (!existing.has_parent_path() || !existing.has_filename()) break;
    tail.push_back(existing.filename());
    existing = existing.parent_path();
  }
  return abs.lexically_normal();
}

bool treeContains(const fsys::path& dir) {
  std::error_code ec;
  return fsys::is_regular_file(dir / kTreeMarker, ec);
}

}  // namespace

Result<void> ensureOutsideRepo(const std::string& path) {
  if (path.empty())
    return Result<void>::fail(Error::make("policy", 1, "empty output path"));
  std::error_code ec;
  fsys::path resolved = resolveBestEffort(fsys::path(path), ec);
  if (ec)
    return Result<void>::fail(Error::make("policy", 2, "cannot resolve path"));
  for (fsys::path a = resolved; !a.empty(); a = a.parent_path()) {
    if (treeContains(a)) {
      return Result<void>::fail(Error::make(
          "policy", 3,
          "refusing to write game-derived data inside the source tree (" +
              a.string() + "). Keep caches, reports and dumps outside the repository"));
    }
    if (!a.has_parent_path() || a == a.parent_path()) break;
  }
  return Result<void>::ok();
}

}  // namespace fs
}  // namespace sb
