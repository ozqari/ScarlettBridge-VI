#pragma once
// Host-side platform services for synthetic test modules: checked memory,
// monotonic clock, synchronization primitives, and a READ-ONLY file service
// confined to an approved root directory (canonicalize + containment check;
// anything outside is denied, never silently allowed).
#include <cstdint>
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {
namespace services {

class MemoryService {
 public:
  Result<void*> allocate(std::size_t bytes);
  void release(void* p, std::size_t bytes);
  std::uint64_t allocatedBytes() const { return allocated_; }
 private:
  std::uint64_t allocated_ = 0;
};

class ClockService {
 public:
  static std::uint64_t nowNanos();  // monotonic
};

#if defined(_WIN32)
class SyncService {
 public:
  SyncService();
  ~SyncService();
  SyncService(const SyncService&) = delete;
  SyncService& operator=(const SyncService&) = delete;
  void lock();
  void unlock();
 private:
  void* srw_ = nullptr;  // PSRWLOCK, opaque to keep the header portable
};
#else
class SyncService {
 public:
  void lock();
  void unlock();
 private:
  void* mu_ = nullptr;
};
#endif

class ReadOnlyFileService {
 public:
  explicit ReadOnlyFileService(std::string approvedRoot) : root_(std::move(approvedRoot)) {}
  // Opens for read only if the canonical path stays inside the approved root.
  Result<std::vector<std::uint8_t>> readFile(const std::string& relPath,
                                             std::size_t maxBytes);
 private:
  std::string root_;
};

}  // namespace services
}  // namespace sb
