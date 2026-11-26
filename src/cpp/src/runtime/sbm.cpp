#include "runtime/sbm.h"
#include "core/version.h"
#include "formats/bounded_reader.h"

namespace sb {
namespace sbm {

Result<Module> parse(const std::uint8_t* data, std::size_t len) {
  BoundedReader r(data, len);
  auto magic = r.u32le();
  if (!magic.ok()) return Result<Module>::fail(magic.error());
  if (magic.value() != kSbmMagic)
    return Result<Module>::fail(Error::make("sbm", 1, "bad magic (not an SBM1 module)"));
  auto headerSize = r.u16le();
  auto version = r.u16le();
  auto importCount = r.u32le();
  if (!headerSize.ok() || !version.ok() || !importCount.ok())
    return Result<Module>::fail(Error::make("sbm", 2, "truncated header"));
  if (version.value() != kSbmFormatVersion)
    return Result<Module>::fail(Error::make("sbm", 3, "unsupported SBM format version"));
  if (importCount.value() > 4096)
    return Result<Module>::fail(Error::make("sbm", 4, "import count over cap"));
  Module m;
  m.formatVersion = version.value();
  for (std::uint32_t i = 0; i < importCount.value(); ++i) {
    auto nameLen = r.u8();
    if (!nameLen.ok()) return Result<Module>::fail(nameLen.error());
    if (nameLen.value() == 0 || nameLen.value() > 128)
      return Result<Module>::fail(Error::make("sbm", 5, "bad import name length"));
    auto name = r.bytes(nameLen.value());
    if (!name.ok()) return Result<Module>::fail(name.error());
    auto minVer = r.u16le();
    if (!minVer.ok()) return Result<Module>::fail(minVer.error());
    m.imports.push_back(Import{name.value(), minVer.value()});
  }
  auto codeSize = r.u32le();
  if (!codeSize.ok()) return Result<Module>::fail(codeSize.error());
  if (codeSize.value() > 64u * 1024u * 1024u)
    return Result<Module>::fail(Error::make("sbm", 6, "code section over 64 MiB cap"));
  auto code = r.bytes(codeSize.value());
  if (!code.ok()) return Result<Module>::fail(code.error());
  const std::string& cs = code.value();
  m.code.assign(cs.begin(), cs.end());
  return Result<Module>::ok(std::move(m));
}

Result<std::vector<std::uint8_t>> write(const std::vector<Import>& imports,
                                        const std::vector<std::uint8_t>& code) {
  if (imports.size() > 4096)
    return Result<std::vector<std::uint8_t>>::fail(Error::make("sbm", 7, "too many imports"));
  if (code.size() > 64u * 1024u * 1024u)
    return Result<std::vector<std::uint8_t>>::fail(Error::make("sbm", 6, "code section over 64 MiB cap"));
  std::vector<std::uint8_t> out;
  auto push32 = [&](std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 24));
  };
  push32(kSbmMagic);
  out.push_back(12); out.push_back(0);  // headerSize (informational)
  out.push_back(static_cast<std::uint8_t>(kSbmFormatVersion)); out.push_back(0);
  push32(static_cast<std::uint32_t>(imports.size()));
  for (const auto& im : imports) {
    if (im.name.empty() || im.name.size() > 128)
      return Result<std::vector<std::uint8_t>>::fail(Error::make("sbm", 5, "bad import name length"));
    out.push_back(static_cast<std::uint8_t>(im.name.size()));
    for (char c : im.name) out.push_back(static_cast<std::uint8_t>(c));
    out.push_back(static_cast<std::uint8_t>(im.minVersion));
    out.push_back(static_cast<std::uint8_t>(im.minVersion >> 8));
  }
  push32(static_cast<std::uint32_t>(code.size()));
  out.insert(out.end(), code.begin(), code.end());
  return Result<std::vector<std::uint8_t>>::ok(std::move(out));
}

}  // namespace sbm
}  // namespace sb
