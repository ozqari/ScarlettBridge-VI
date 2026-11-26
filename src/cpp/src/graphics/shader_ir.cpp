#include "graphics/shader_ir.h"
#include <cstdlib>
#include <sstream>

namespace sb {
namespace sbvs {
namespace {
bool isReg(std::uint8_t r) { return r < kRegs; }
}  // namespace

Result<Program> parseText(const std::string& text) {
  Program p;
  std::istringstream in(text);
  std::string line;
  auto reg = [](const std::string& t, std::uint8_t& out) -> bool {
    if (t == "OUT") { out = kOut; return true; }
    if (t.size() == 2 && t[0] == 'r' && t[1] >= '0' && t[1] <= '7') {
      out = static_cast<std::uint8_t>(t[1] - '0');
      return true;
    }
    return false;
  };
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream ls(line);
    std::string a, b, c, d;
    ls >> a;
    if (a.empty() || a[0] == '#') continue;
    if (a[0] == 'c' && a.size() > 1) {
      // cN = (x, y, z, w)
      int idx = std::atoi(a.c_str() + 1);
      if (idx < 0 || idx > 15)
        return Result<Program>::fail(Error::make("shader", 1, "bad const index: " + a));
      char eq = 0, lp = 0;
      float x = 0, y = 0, z = 0, w = 0;
      char comma = 0, rp = 0;
      ls >> eq >> lp >> x >> comma >> y >> comma >> z >> comma >> w >> rp;
      if (eq != '=' || lp != '(' || rp != ')')
        return Result<Program>::fail(Error::make("shader", 2, "bad const line: " + line));
      if (static_cast<int>(p.consts.size()) != idx)
        return Result<Program>::fail(Error::make("shader", 3, "consts must be declared in order"));
      p.consts.push_back({x, y, z, w});
      continue;
    }
    Instr ins;
    if (a == "CONST") {
      ls >> b >> c;
      std::uint8_t dst = 0;
      if (!reg(b, dst) || c.size() < 2 || c[0] != 'c')
        return Result<Program>::fail(Error::make("shader", 4, "bad CONST: " + line));
      ins.op = Op::Const;
      ins.dst = dst;
      ins.constIdx = static_cast<std::uint8_t>(std::atoi(c.c_str() + 1));
    } else {
      ls >> b >> c >> d;
      if (a == "MOV") ins.op = Op::Mov;
      else if (a == "ADD") ins.op = Op::Add;
      else if (a == "MUL") ins.op = Op::Mul;
      else if (a == "MAD") ins.op = Op::Mad;
      else return Result<Program>::fail(Error::make("shader", 5, "bad opcode: " + a));
      std::uint8_t dst = 0, s0 = 0, s1 = 0, s2 = 0;
      if (!reg(b, dst) || !reg(c, s0))
        return Result<Program>::fail(Error::make("shader", 6, "bad operands: " + line));
      ins.dst = dst;
      ins.src0 = s0;
      if (ins.op == Op::Add || ins.op == Op::Mul || ins.op == Op::Mad) {
        if (!reg(d.empty() ? c : d, s1))
          return Result<Program>::fail(Error::make("shader", 6, "bad operands: " + line));
        // Note: "ADD r1 r1 c" forms are rejected; keep fixtures register-only.
        ins.src1 = s1;
      }
      if (ins.op == Op::Mad) {
        std::string e;
        ls >> e;
        if (!reg(e, s2))
          return Result<Program>::fail(Error::make("shader", 6, "MAD needs 3 sources: " + line));
        ins.src2 = s2;
      }
    }
    p.code.push_back(ins);
    if (p.code.size() > 256)
      return Result<Program>::fail(Error::make("shader", 7, "program over 256-instruction cap"));
  }
  return Result<Program>::ok(std::move(p));
}

Result<void> verify(const Program& p) {
  if (p.consts.size() > 16)
    return Result<void>::fail(Error::make("shader", 10, "too many consts"));
  if (p.code.empty() || p.code.size() > 256)
    return Result<void>::fail(Error::make("shader", 11, "bad code size"));
  bool defined[kRegs] = {false, false};  // r0 = input c0 slot, rest undefined
  defined[0] = true;
  bool outWritten = false;
  for (const Instr& ins : p.code) {
    if (!isReg(ins.dst))
      return Result<void>::fail(Error::make("shader", 12, "destination out of range"));
    if (ins.op == Op::Const) {
      if (ins.constIdx >= p.consts.size())
        return Result<void>::fail(Error::make("shader", 13, "const index out of range"));
      if (ins.dst == kOut)
        return Result<void>::fail(Error::make("shader", 14, "CONST may not target OUT directly"));
    } else {
      if (!isReg(ins.src0) || !defined[ins.src0])
        return Result<void>::fail(Error::make("shader", 15, "use of undefined register"));
      if ((ins.op == Op::Add || ins.op == Op::Mul || ins.op == Op::Mad)) {
        if (!isReg(ins.src1) || !defined[ins.src1])
          return Result<void>::fail(Error::make("shader", 15, "use of undefined register"));
      }
      if (ins.op == Op::Mad && (!isReg(ins.src2) || !defined[ins.src2]))
        return Result<void>::fail(Error::make("shader", 15, "use of undefined register"));
    }
    defined[ins.dst] = true;
    if (ins.dst == kOut) outWritten = true;
  }
  if (!outWritten)
    return Result<void>::fail(Error::make("shader", 16, "OUT is never written"));
  return Result<void>::ok();
}

Result<std::array<float, 4>> interpret(const Program& p, const std::array<float, 4>& in) {
  if (auto v = verify(p); !v.ok()) return Result<std::array<float, 4>>::fail(v.error());
  std::array<float, 4> regs[kRegs] = {};
  regs[0] = in;
  for (const Instr& ins : p.code) {
    auto& d = regs[ins.dst];
    switch (ins.op) {
      case Op::Const: d = p.consts[ins.constIdx]; break;
      case Op::Mov: d = regs[ins.src0]; break;
      case Op::Add:
        for (int i = 0; i < 4; ++i) d[i] = regs[ins.src0][i] + regs[ins.src1][i];
        break;
      case Op::Mul:
        for (int i = 0; i < 4; ++i) d[i] = regs[ins.src0][i] * regs[ins.src1][i];
        break;
      case Op::Mad:
        for (int i = 0; i < 4; ++i)
          d[i] = regs[ins.src0][i] * regs[ins.src1][i] + regs[ins.src2][i];
        break;
    }
  }
  return Result<std::array<float, 4>>::ok(regs[kOut]);
}

}  // namespace sbvs
}  // namespace sb
