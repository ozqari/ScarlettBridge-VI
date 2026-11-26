#pragma once
// SBVS -> HLSL translation + compile validation. Stage-independent ALU
// sequence; the emitter produces a VS (passthrough position) and a PS (the
// program). Validation compiles with D3DCompile (OS component). DXC loading
// (dxcompiler.dll) is an explicit Unsupported until Phase 7 lands it with
// versioning + tests — no silent substitution.
#include <cstdint>
#include <string>
#include <vector>
#include "core/result.h"
#include "graphics/shader_ir.h"

namespace sb {
namespace shader {

struct Translation {
  std::string hlsl;
  std::vector<std::uint8_t> dxbc;  // validated PS bytecode (may be empty if skipped)
  std::vector<std::string> warnings;
};

Result<Translation> translate(const sbvs::Program& program, bool debugLayer);
Result<Translation> translateText(const std::string& text, bool debugLayer);

}  // namespace shader
}  // namespace sb
