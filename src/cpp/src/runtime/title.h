#pragma once
// Title manifest + mapped-image registry for synthetic modules.
//
// Idea informed by victory-hle runtime.rs (Title::from_manifest XML element
// scan with entity decoding + comment/CDATA skipping, storage quota from
// PersistentLocalStorage/SizeMB, saves() beside storage; image path tracking
// by base address + case-insensitive file-name lookup). Independently
// implemented here in portable C++; no code copied.
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {

struct TitleManifest {
  std::uint32_t titleId = 0;
  std::string storeId;
  std::string storageDir;
  std::uint64_t storageBytes = 0;
  // Saves live next to persistent storage, never in the cache.
  std::string savesDir() const;
  static TitleManifest parse(const std::string& xmlText);
};

class ImageRegistry {
 public:
  void setMainImage(std::uint64_t base) {
    std::lock_guard<std::mutex> l(mu_);
    mainBase_ = base;
  }
  std::uint64_t mainImage() const {
    std::lock_guard<std::mutex> l(mu_);
    return mainBase_;
  }
  void setImagePath(std::uint64_t base, const std::string& path);
  void forgetImage(std::uint64_t base);
  std::string imagePath(std::uint64_t base) const;
  // Base of the image whose file name matches (case-insensitive), else 0.
  std::uint64_t imageNamed(const std::string& fileName) const;

 private:
  mutable std::mutex mu_;
  std::uint64_t mainBase_ = 0;
  std::map<std::uint64_t, std::string> paths_;
};

}  // namespace sb
