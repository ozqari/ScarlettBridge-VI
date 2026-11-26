#pragma once
// SBVS: our own tiny synthetic shader ISA for fixture testing (NOT a console
// ISA, NOT copied from any project). Registers r0..r7 + OUT(8). A program is
// a straight-line ALU sequence computing OUT from input c0.
//
// Text form (fixtures):
//   c0 = (1.0, 0.25, 0.1, 1.0)
//   CONST r1 c0
//   MOV OUT r1
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {
namespace sbvs {

enum class Op : std::uint8_t { Const, Mov, Add, Mul, Mad };
enum : std::uint8_t { kOut = 8, kRegs = 9 };

struct Instr {
  Op op = Op::Mov;
  std::uint8_t dst = 0;     // 0..8
  std::uint8_t src0 = 0;    // reg or (Const) const-index
  std::uint8_t src1 = 0;    // reg (Add/Mul/Mad)
  std::uint8_t src2 = 0;    // reg (Mad only)
  std::uint8_t constIdx = 0;  // valid when op == Const
};

struct Program {
  std::vector<std::array<float, 4>> consts;
  std::vector<Instr> code;  // <= 256 instructions
};

Result<Program> parseText(const std::string& text);
Result<void> verify(const Program& p);
// Deterministic CPU reference: interprets the program for input c0.
Result<std::array<float, 4>> interpret(const Program& p, const std::array<float, 4>& in);

}  // namespace sbvs
}  // namespace sb
