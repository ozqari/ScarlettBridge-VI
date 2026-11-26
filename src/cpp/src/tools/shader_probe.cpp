// sb_shader_probe: parse a .sbvs fixture, verify, run the CPU reference,
// translate to HLSL, and validate-compile it. Compares CPU vs expected pixel.
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include "graphics/shader_translate.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: sb_shader_probe <fixture.sbvs>\n";
    return 2;
  }
  std::ifstream f(argv[1], std::ios::binary);
  if (!f) {
    std::cerr << "{\"ok\":false,\"error\":\"cannot open fixture\"}\n";
    return 1;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  auto prog = sb::sbvs::parseText(ss.str());
  if (!prog.ok()) {
    std::cout << "{\"ok\":false,\"error\":\"" << prog.error().message << "\"}\n";
    return 1;
  }
  std::array<float, 4> in = {1.0f, 0.25f, 0.1f, 1.0f};
  auto ref = sb::sbvs::interpret(prog.value(), in);
  if (!ref.ok()) {
    std::cout << "{\"ok\":false,\"error\":\"" << ref.error().message << "\"}\n";
    return 1;
  }
  auto tr = sb::shader::translate(prog.value(), false);
  if (!tr.ok()) {
    std::cout << "{\"ok\":false,\"error\":\"" << tr.error().message << "\"}\n";
    return 1;
  }
  if (tr.value().dxbc.empty()) {
    std::cout << "{\"skip\":true,\"hlslBytes\":" << tr.value().hlsl.size() << "}\n";
    return 77;  // parsed + emitted, no compile validation on this machine
  }
  const auto& px = ref.value();
  std::cout << "{\"ok\":true,\"cpuPixel\":[" << px[0] << "," << px[1] << "," << px[2] << ","
            << px[3] << "],\"hlslBytes\":" << tr.value().hlsl.size()
            << ",\"dxbcBytes\":" << tr.value().dxbc.size() << "}\n";
  return (std::fabs(px[0] - 1.0f) < 1e-5 && std::fabs(px[3] - 1.0f) < 1e-5) ? 0 : 1;
}
