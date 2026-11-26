#include "graphics/shader_translate.h"
#include <cstdio>
#include <cstring>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3dcompiler.h>
#include <wrl/client.h>
#endif

namespace sb {
namespace shader {
namespace {
std::string regName(std::uint8_t r) {
  if (r == sbvs::kOut) return std::string("OUT");
  char buf[4];
  std::snprintf(buf, sizeof(buf), "r%u", r);
  return std::string(buf);
}
}  // namespace

Result<Translation> translate(const sbvs::Program& program, bool debugLayer) {
  if (auto v = sbvs::verify(program); !v.ok())
    return Result<Translation>::fail(v.error());
  Translation t;
  std::ostringstream hs;
  hs << "// SBVS translator v" << kTranslatorVersion << " (synthetic fixtures only)\n";
  hs << "cbuffer SBVSConsts : register(b0) {\n";
  for (std::size_t i = 0; i < program.consts.size(); ++i) {
    const auto& c = program.consts[i];
    hs << "  float4 c" << i << ";\n";
    (void)c;
  }
  hs << "};\n";
  // NOTE: const values are baked below (fixtures are static); the cbuffer
  // declaration documents the binding the real path will use.
  hs << "struct VOut { float4 pos : SV_Position; };\n";
  hs << "VOut VS(uint vid : SV_VertexID) {\n"
        "  float2 p[3] = { float2(-1,-1), float2(3,-1), float2(-1,3) };\n"
        "  VOut o; o.pos = float4(p[vid], 0, 1); return o; }\n";
  hs << "float4 PS(VOut i : TEXCOORD0) : SV_Target {\n";
  for (std::size_t i = 0; i < program.consts.size(); ++i) {
    const auto& c = program.consts[i];
    hs << "  float4 c" << i << " = float4(" << c[0] << ", " << c[1] << ", " << c[2] << ", " << c[3]
       << ");\n";
  }
  hs << "  float4 r0 = float4(1.0, 0.25, 0.1, 1.0);\n";
  for (std::size_t k = 0; k < 8; ++k) {
    if (k == 0) continue;
    hs << "  float4 r" << k << " = float4(0,0,0,0);\n";
  }
  hs << "  float4 OUT = float4(0,0,0,0);\n";
  for (const auto& ins : program.code) {
    std::string d = regName(ins.dst);
    switch (ins.op) {
      case sbvs::Op::Const: hs << "  " << d << " = c" << (int)ins.constIdx << ";\n"; break;
      case sbvs::Op::Mov: hs << "  " << d << " = " << regName(ins.src0) << ";\n"; break;
      case sbvs::Op::Add:
        hs << "  " << d << " = " << regName(ins.src0) << " + " << regName(ins.src1) << ";\n";
        break;
      case sbvs::Op::Mul:
        hs << "  " << d << " = " << regName(ins.src0) << " * " << regName(ins.src1) << ";\n";
        break;
      case sbvs::Op::Mad:
        hs << "  " << d << " = " << regName(ins.src0) << " * " << regName(ins.src1) << " + "
           << regName(ins.src2) << ";\n";
        break;
    }
  }
  hs << "  return OUT;\n}\n";
  t.hlsl = hs.str();

#if defined(_WIN32)
  UINT flags = D3DCOMPILE_WARNINGS_ARE_ERRORS;
  if (debugLayer) flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
  Microsoft::WRL::ComPtr<ID3DBlob> code, errors;
  HRESULT hr = D3DCompile(t.hlsl.c_str(), t.hlsl.size(), "sbvs.ps", nullptr, nullptr,
                          "PS", "ps_5_0", flags, 0, &code, &errors);
  if (FAILED(hr)) {
    std::string log = errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()),
                                           errors->GetBufferSize())
                             : "unknown";
    return Result<Translation>::fail(
        Error::make("shader", 20, "HLSL validation compile failed: " + log));
  }
  const auto* p = static_cast<const std::uint8_t*>(code->GetBufferPointer());
  t.dxbc.assign(p, p + code->GetBufferSize());
#else
  t.warnings.push_back("D3DCompile validation skipped: non-Windows build");
#endif
  return Result<Translation>::ok(std::move(t));
}

Result<Translation> translateText(const std::string& text, bool debugLayer) {
  auto p = sbvs::parseText(text);
  if (!p.ok()) return Result<Translation>::fail(p.error());
  return translate(p.value(), debugLayer);
}

}  // namespace shader
}  // namespace sb
