#pragma once
// Every binary-data path goes through this cursor. All reads are bounds-
// checked with overflow-safe arithmetic; short/malformed input is an Error,
// never a partial success.
#include <cstdint>
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {

class BoundedReader {
 public:
  BoundedReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
  explicit BoundedReader(const std::vector<std::uint8_t>& v)
      : data_(v.empty() ? nullptr : v.data()), size_(v.size()) {}

  std::size_t remaining() const { return size_ - pos_; }
  std::size_t position() const { return pos_; }

  Result<std::uint8_t> u8();
  Result<std::uint16_t> u16le();
  Result<std::uint32_t> u32le();
  Result<std::uint64_t> u64le();
  Result<std::string> bytes(std::size_t len);  // raw bytes as string
  Result<void> skip(std::size_t len);

 private:
  Result<void> reserve(std::size_t len);
  const std::uint8_t* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t pos_ = 0;
};

}  // namespace sb
