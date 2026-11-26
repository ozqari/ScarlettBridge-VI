#include "formats/bounded_reader.h"

namespace sb {

Result<void> BoundedReader::reserve(std::size_t len) {
  if (len > size_ || pos_ > size_ - len)
    return Result<void>::fail(Error::make("format", 1, "read past end of buffer"));
  return Result<void>::ok();
}

Result<std::uint8_t> BoundedReader::u8() {
  if (auto r = reserve(1); !r.ok()) return Result<std::uint8_t>::fail(r.error());
  return Result<std::uint8_t>::ok(data_[pos_++]);
}

Result<std::uint16_t> BoundedReader::u16le() {
  if (auto r = reserve(2); !r.ok()) return Result<std::uint16_t>::fail(r.error());
  std::uint16_t v = static_cast<std::uint16_t>(data_[pos_]) |
                    (static_cast<std::uint16_t>(data_[pos_ + 1]) << 8);
  pos_ += 2;
  return Result<std::uint16_t>::ok(v);
}

Result<std::uint32_t> BoundedReader::u32le() {
  if (auto r = reserve(4); !r.ok()) return Result<std::uint32_t>::fail(r.error());
  std::uint32_t v = static_cast<std::uint32_t>(data_[pos_]) |
                    (static_cast<std::uint32_t>(data_[pos_ + 1]) << 8) |
                    (static_cast<std::uint32_t>(data_[pos_ + 2]) << 16) |
                    (static_cast<std::uint32_t>(data_[pos_ + 3]) << 24);
  pos_ += 4;
  return Result<std::uint32_t>::ok(v);
}

Result<std::uint64_t> BoundedReader::u64le() {
  if (auto r = reserve(8); !r.ok()) return Result<std::uint64_t>::fail(r.error());
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(data_[pos_ + i]) << (8 * i);
  pos_ += 8;
  return Result<std::uint64_t>::ok(v);
}

Result<std::string> BoundedReader::bytes(std::size_t len) {
  if (auto r = reserve(len); !r.ok()) return Result<std::string>::fail(r.error());
  std::string s(reinterpret_cast<const char*>(data_ + pos_), len);
  pos_ += len;
  return Result<std::string>::ok(std::move(s));
}

Result<void> BoundedReader::skip(std::size_t len) {
  if (auto r = reserve(len); !r.ok()) return r;
  pos_ += len;
  return Result<void>::ok();
}

}  // namespace sb
