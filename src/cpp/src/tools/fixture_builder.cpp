// sb_fixture_builder: generates project-owned synthetic fixtures into
// tests/fixtures/: SBM1 modules and SBVS shaders. No commercial content.
#include <filesystem>
#include <fstream>
#include <iostream>
#include "core/hash.h"
#include "runtime/sbm.h"

namespace {
bool writeFile(const std::filesystem::path& p, const std::vector<std::uint8_t>& bytes) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  f.write(reinterpret_cast<const char*>(bytes.data()),
          static_cast<std::streamsize>(bytes.size()));
  return !!f;
}
bool writeText(const std::filesystem::path& p, const std::string& s) {
  return writeFile(p, std::vector<std::uint8_t>(s.begin(), s.end()));
}
}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path out = "tests/fixtures";
  if (argc >= 2) out = argv[1];
  std::error_code ec;
  std::filesystem::create_directories(out, ec);
  if (ec) {
    std::cerr << "cannot create " << out.string() << "\n";
    return 1;
  }
  // Minimal module: one known import, tiny code payload.
  auto m1 = sb::sbm::write({{"sb.memory.v1", 1}}, {0x01, 0x02, 0x03});
  if (!m1.ok() || !writeFile(out / "minimal.sbm1", m1.value())) return 1;
  // Module with an unknown import (loader must report it, never fake it).
  auto m2 = sb::sbm::write({{"sb.memory.v1", 1}, {"sb.unknown.v9", 1}}, {0x00});
  if (!m2.ok() || !writeFile(out / "unresolved.sbm1", m2.value())) return 1;
  // Constant-color SBVS shader (matches the D3D12 smoke color).
  const std::string sbvs =
      "# constant color (1.0, 0.25, 0.1, 1.0)\n"
      "c0 = (1.0, 0.25, 0.1, 1.0)\n"
      "CONST r1 c0\n"
      "MOV OUT r1\n";
  if (!writeText(out / "constant.sbvs", sbvs)) return 1;
  // ALU variant: exercises ADD/MUL/MAD + verifier def-before-use.
  const std::string alu =
      "c0 = (1.0, 0.25, 0.1, 1.0)\n"
      "c1 = (0.5, 0.5, 0.5, 0.5)\n"
      "CONST r1 c0\n"
      "CONST r2 c1\n"
      "MUL r3 r1 r2\n"
      "ADD r4 r3 r2\n"
      "MAD r5 r1 r2 r4\n"
      "MOV OUT r5\n";
  if (!writeText(out / "alu.sbvs", alu)) return 1;
  std::cout << "fixtures written to " << out.string() << "\n";
  return 0;
}
