#include "runtime/platform_services.h"
#include <chrono>
#include <filesystem>
#include <fstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <mutex>
#endif

namespace sb {
namespace services {

Result<void*> MemoryService::allocate(std::size_t bytes) {
  if (bytes == 0 || bytes > 1024u * 1024u * 1024u)
    return Result<void*>::fail(Error::make("memory", 1, "allocation size out of bounds"));
  if (allocated_ > 4ull * 1024u * 1024u * 1024u - bytes)
    return Result<void*>::fail(Error::make("memory", 2, "host memory budget exceeded"));
  void* p =
#if defined(_WIN32)
      VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
      ::operator new(bytes, std::nothrow);
#endif
  if (!p) return Result<void*>::fail(Error::make("memory", 3, "host allocation failed"));
  allocated_ += bytes;
  return Result<void*>::ok(p);
}

void MemoryService::release(void* p, std::size_t bytes) {
  if (!p) return;
#if defined(_WIN32)
  VirtualFree(p, 0, MEM_RELEASE);
#else
  ::operator delete(p);
#endif
  allocated_ -= (bytes < allocated_ ? bytes : allocated_);
}

std::uint64_t ClockService::nowNanos() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

#if defined(_WIN32)
SyncService::SyncService() {
  auto* l = new SRWLOCK;
  InitializeSRWLock(l);
  srw_ = l;
}
SyncService::~SyncService() { delete static_cast<SRWLOCK*>(srw_); }
void SyncService::lock() { AcquireSRWLockExclusive(static_cast<PSRWLOCK>(srw_)); }
void SyncService::unlock() { ReleaseSRWLockExclusive(static_cast<PSRWLOCK>(srw_)); }
#else
void SyncService::lock() {
  if (!mu_) mu_ = new std::mutex;
  static_cast<std::mutex*>(mu_)->lock();
}
void SyncService::unlock() { static_cast<std::mutex*>(mu_)->unlock(); }
#endif

Result<std::vector<std::uint8_t>> ReadOnlyFileService::readFile(const std::string& relPath,
                                                               std::size_t maxBytes) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path root = fs::weakly_canonical(fs::path(root_), ec);
  if (ec) return Result<std::vector<std::uint8_t>>::fail(Error::make("file", 1, "bad approved root"));
  fs::path full = fs::weakly_canonical(root / fs::path(relPath), ec);
  if (ec) return Result<std::vector<std::uint8_t>>::fail(Error::make("file", 2, "bad path"));
  // Containment check: the canonical target must stay inside the root.
  std::string rs = root.string(), fs_ = full.string();
  if (fs_.size() < rs.size() || fs_.compare(0, rs.size(), rs) != 0)
    return Result<std::vector<std::uint8_t>>::fail(
        Error::make("file", 3, "path escapes approved root (denied)"));
  std::ifstream f(full, std::ios::binary);
  if (!f) return Result<std::vector<std::uint8_t>>::fail(Error::make("file", 4, "cannot open file"));
  std::vector<std::uint8_t> out;
  out.reserve(65536);
  char buf[65536];
  std::size_t total = 0;
  while (f) {
    f.read(buf, sizeof(buf));
    std::size_t got = static_cast<std::size_t>(f.gcount());
    if (total + got > maxBytes)
      return Result<std::vector<std::uint8_t>>::fail(Error::make("file", 5, "file over size cap"));
    out.insert(out.end(), buf, buf + got);
    total += got;
  }
  return Result<std::vector<std::uint8_t>>::ok(std::move(out));
}

}  // namespace services
}  // namespace sb
