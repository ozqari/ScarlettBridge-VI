// sb_tests: CPU-only unit + integration suite (CTest). No GPU, no Qt, no
// commercial content. Failing tests print the reason; exit code = failures.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include "app/cli_parse.h"
#include "core/config.h"
#include "core/filesystem_guards.h"
#include "core/hash.h"
#include "core/json.h"
#include "core/logging.h"
#include "core/logging.h"
#include "core/version.h"
#include "formats/bounded_reader.h"
#include "formats/pe_exports.h"
#include "formats/pe_imports.h"
#include "graphics/adapter_select.h"
#include "graphics/pipeline_cache.h"
#include "graphics/shader_ir.h"
#include "graphics/shader_tally.h"
#include "graphics/shader_translate.h"
#include "ipc/protocol.h"
#include "media/input_script.h"
#include "runtime/console_timer.h"
#include "runtime/forwarder.h"
#include "runtime/host_path.h"
#include "runtime/image_layout.h"
#include "runtime/imports.h"
#include "runtime/init_order.h"
#include "runtime/lifecycle.h"
#include "runtime/resolve.h"
#include "runtime/routes.h"
#include "runtime/sbm.h"
#include "runtime/stubs.h"
#include "runtime/title.h"
#include "telemetry/fault_filter.h"
#include "telemetry/driver_watch.h"
#include "telemetry/engine_stats.h"
#include "telemetry/frame_box.h"
#include "telemetry/output_compare.h"
#include "telemetry/metrics.h"
#include "telemetry/refusal_census.h"
#include "telemetry/trace_filter.h"
#include "ui/backend_status.h"

namespace {
int g_fail = 0;
int g_pass = 0;
void check(bool cond, const char* name, const std::string& detail = "") {
  if (cond) {
    ++g_pass;
    std::printf("ok   %s\n", name);
  } else {
    ++g_fail;
    std::printf("FAIL %s %s\n", name, detail.c_str());
  }
}
}  // namespace

int main() {
  // --- json ---
  {
    auto v = sb::json::Value::parse("{\"a\":1,\"b\":[true,null]}");
    check(v.ok() && v.value().at("a").asNumber() == 1.0, "json.parse.object");
    check(!sb::json::Value::parse("{\"a\":}").ok(), "json.reject.trailing-comma");
    check(!sb::json::Value::parse("{\"a\":1} garbage").ok(), "json.reject.trailing-garbage");
    check(!sb::json::Value::parse("[").ok(), "json.reject.unterminated");
    sb::json::Value o = sb::json::Value::object();
    o.set("x", sb::json::Value::number(1.5));
    auto back = sb::json::Value::parse(o.serialize());
    check(back.ok() && back.value().at("x").asNumber() == 1.5, "json.roundtrip");
  }
  // --- hashing (known vectors) ---
  {
    const char* abc = "abc";
    check(sb::sha256hex(abc, 3) ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "hash.sha256.abc");
    check(sb::fnv1a32("") == 0x811c9dc5u, "hash.fnv.empty");
    std::string hello = "hello";
    check(sb::fnv1a32(hello) == 0x4f9f2cabu, "hash.fnv.hello");
  }
  // --- config ---
  {
    sb::Settings d = sb::defaults();
    check(d.schema == sb::kConfigSchemaVersion && d.vsync, "config.defaults");
    auto bad = sb::json::Value::parse("{\"schema\":1,\"vsync\":\"x\",\"frameCap\":-5}");
    check(bad.ok(), "config.parse.tolerant");
    auto loaded = sb::fromJson(bad.value());
    check(loaded.settings.schema == 3 && loaded.settings.vsync &&
              loaded.settings.frameCap == 0,
          "config.migrate");
    check(!loaded.warnings.empty(), "config.warnings.reported");
  }
  // --- ipc protocol ---
  {
    check(sb::ipc::isKnownType("FrameStats") && !sb::ipc::isKnownType("Nope"),
          "ipc.known-types");
    auto enc = sb::ipc::encode(sb::ipc::hello("t"));
    check(enc.ok(), "ipc.encode.hello");
    auto dec = sb::ipc::decode(enc.value().data(), enc.value().size());
    check(dec.ok(), "ipc.roundtrip");
    sb::json::Value bad = sb::json::Value::object();
    bad.set("v", sb::json::Value::number(999));
    bad.set("type", sb::json::Value::str("Hello"));
    check(!sb::ipc::validate(bad).ok(), "ipc.reject.version-mismatch");
    sb::json::Value trunc = sb::json::Value::object();
    check(!sb::ipc::decode(reinterpret_cast<const std::uint8_t*>("ab"), 2).ok(),
          "ipc.reject.truncated");
    std::vector<std::uint8_t> big(4 + sb::kIpcMaxMessageBytes + 1, 0);
    check(!sb::ipc::decode(big.data(), big.size()).ok(), "ipc.reject.oversize");
    sb::json::Value noFields = sb::json::Value::object();
    noFields.set("v", sb::json::Value::number(1));
    noFields.set("type", sb::json::Value::str("LogEvent"));
    check(!sb::ipc::validate(noFields).ok(), "ipc.reject.missing-fields");
  }
  // --- lifecycle ---
  {
    sb::Lifecycle lc;
    check(lc.transition(sb::RuntimeState::Validating, "t").ok(), "lifecycle.legal");
    check(!lc.transition(sb::RuntimeState::Running, "skip").ok(), "lifecycle.illegal-rejected");
    check(lc.transition(sb::RuntimeState::Failed, "boom").ok(), "lifecycle.any-failed");
    check(lc.transition(sb::RuntimeState::Validating, "retry").ok(), "lifecycle.failed-restart");
  }
  // --- bounded reader ---
  {
    std::vector<std::uint8_t> v = {1, 2, 3};
    sb::BoundedReader r(v);
    check(r.u16le().value() == 0x0201, "reader.u16le");
    check(!r.bytes(5).ok(), "reader.reject.oob");
    check(!r.skip(5).ok(), "reader.reject.skip-oob");
  }
  // --- sbm loader ---
  {
    auto good = sb::sbm::write({{"sb.memory.v1", 1}}, {0xAA});
    check(good.ok(), "sbm.write");
    auto mod = sb::sbm::parse(good.value().data(), good.value().size());
    check(mod.ok() && mod.value().imports.size() == 1, "sbm.parse.valid");
    std::vector<std::uint8_t> bad = {0, 0, 0, 0};
    check(!sb::sbm::parse(bad.data(), bad.size()).ok(), "sbm.reject.bad-magic");
    std::vector<std::uint8_t> trunc = {'S', 'B', 'M', '1', 12};
    check(!sb::sbm::parse(trunc.data(), trunc.size()).ok(), "sbm.reject.truncated");
    sb::ImportRegistry reg;
    reg.registerService("sb.memory.v1", 1);
    check(reg.resolve("m", "sb.memory.v1", 1).ok(), "imports.resolve.known");
    check(!reg.resolve("m", "sb.nope.v1", 1).ok(), "imports.reject.unknown");
    check(!reg.unresolved().empty(), "imports.unresolved.recorded");
    check(!reg.resolve("m", "sb.memory.v1", 9).ok(), "imports.reject.old-version");
  }
  // --- pipeline cache ---
  {
    std::string k1 = sb::cache::entryKey("d3d12", "aa", "bb");
    std::string k2 = sb::cache::entryKey("d3d12", "aa", "bb");
    check(k1 == k2 && k1.size() == 64, "cache.key.stable");
    check(sb::cache::entryKey("vulkan", "aa", "bb") != k1, "cache.key.backend-matters");
    std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "sb-cache-test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    sb::cache::ShaderCache cache(dir.string(), 8, 1024 * 1024);
    std::vector<std::uint8_t> payload = {9, 8, 7};
    check(cache.store(k1, payload).ok(), "cache.store");
    auto hit = cache.load(k1);
    check(hit.ok() && hit.value().payload == payload, "cache.load.hit");
    // Corrupt the entry: valid name, garbage content.
    {
      std::string k3 = sb::cache::entryKey("d3d12", "cc", "dd");
      std::vector<std::uint8_t> junk = {'x'};
      check(cache.store(k3, junk).ok(), "cache.store.junk");
      std::filesystem::path p = dir / (k3 + ".bin");
      FILE* f = std::fopen(p.string().c_str(), "r+b");
      if (f) {
        std::fputc('Z', f);
        std::fclose(f);
      }
      auto bad = cache.load(k3);
      check(!bad.ok() && cache.wasCorruptDiscard(), "cache.corrupt.discarded");
    }
    std::filesystem::remove_all(dir, ec);
  }
  // --- sbvs verifier + interpreter + translator ---
  {
    auto prog = sb::sbvs::parseText("c0 = (1.0, 0.25, 0.1, 1.0)\nCONST r1 c0\nMOV OUT r1\n");
    check(prog.ok() && sb::sbvs::verify(prog.value()).ok(), "sbvs.valid");
    std::array<float, 4> in = {1, 1, 1, 1};
    auto px = sb::sbvs::interpret(prog.value(), in);
    check(px.ok() && px.value()[0] == 1.0f && px.value()[1] == 0.25f, "sbvs.interpret");
    auto bad = sb::sbvs::parseText("MOV OUT r7\n");
    check(bad.ok() && !sb::sbvs::verify(bad.value()).ok(), "sbvs.reject.undefined-use");
    auto badOp = sb::sbvs::parseText("FROB OUT r1\n");
    check(!badOp.ok(), "sbvs.reject.bad-opcode");
    auto tr = sb::shader::translate(prog.value(), false);
    check(tr.ok() && !tr.value().hlsl.empty(), "shader.translate.hlsl");
  }

  // --- host policy + census + stub modes ---
  {
    sb::ImportRegistry reg;
    reg.registerService("sb.memory.v1", 1);
    check(reg.census("m", "sb.memory.v1").klass == sb::ImportClass::Ours,
          "census.ours");
    // Default-deny: unlisted host DLL is denied, never passed through.
    check(reg.census("m", "kernel32.dll").klass == sb::ImportClass::HostDenied,
          "census.host-denied-by-default");
    sb::HostPolicy pol;
    pol.allowHostModule("kernel32.dll");
    reg.setHostPolicy(pol);
    check(reg.census("m", "kernel32.dll").klass == sb::ImportClass::HostAllowed,
          "census.host-allowed");
    check(reg.census("m", "sb.nope.v1").klass == sb::ImportClass::Missing,
          "census.missing");
    reg.setStubMode(sb::StubMode::Trap);
    check(reg.census("m", "sb.nope.v1").klass == sb::ImportClass::StubTrap,
          "census.stub-trap");
    check(sb::toString(sb::ImportClass::Ours) == "ours", "importclass.names");
    // resolve() never fakes success, regardless of stub mode.
    check(!reg.resolve("m", "sb.nope.v1", 1).ok(), "imports.reject.unknown.trap-mode");
  }
  // --- redaction ---
  {
    std::string dirty = "dump at C:\\Users\\bob\\game\\x.sav and /home/bob/x";
    std::string clean = sb::log::redactPaths(dirty);
    check(clean.find("bob") == std::string::npos, "redact.paths");
    auto fr = sb::diag::formatFaultReport(0xC0000005, reinterpret_cast<const void*>(0x140001234),
                                          "title.exe");
    check(fr.find("C0000005") != std::string::npos && fr.find("title.exe") != std::string::npos,
          "crash.format");
  }
  // --- sbm roundtrip ---
  {
    auto w = sb::sbm::write({{"sb.memory.v1", 1}, {"sb.clock.v1", 2}}, {0xDE, 0xAD});
    check(w.ok(), "sbm.write.roundtrip");
    auto p = sb::sbm::parse(w.value().data(), w.value().size());
    check(p.ok() && p.value().imports.size() == 2 &&
              p.value().imports[1].minVersion == 2 && p.value().code.size() == 2,
          "sbm.parse.roundtrip");
  }
  // --- sbvs alu reference values ---
  {
    auto prog = sb::sbvs::parseText(
        "c0 = (1.0, 0.25, 0.1, 1.0)\nc1 = (0.5, 0.5, 0.5, 0.5)\n"
        "CONST r1 c0\nCONST r2 c1\nMUL r3 r1 r2\nADD r4 r3 r2\nMAD r5 r1 r2 r4\nMOV OUT r5\n");
    check(prog.ok(), "sbvs.alu.parse");
    std::array<float, 4> in = {0, 0, 0, 0};
    auto px = sb::sbvs::interpret(prog.value(), in);
    // r3=(.5,.125,.05,.5) r4=(1,.625,.55,1) OUT=r3? no: OUT=r1*r2+r4=(1.5,.75,.6,1.5)
    check(px.ok() && std::fabs(px.value()[0] - 1.5f) < 1e-5 &&
              std::fabs(px.value()[1] - 0.75f) < 1e-5 &&
              std::fabs(px.value()[2] - 0.6f) < 1e-5 &&
              std::fabs(px.value()[3] - 1.5f) < 1e-5,
          "sbvs.alu.values");
  }
  // --- lifecycle history ---
  {
    sb::Lifecycle lc;
    lc.transition(sb::RuntimeState::Validating, "a");
    lc.transition(sb::RuntimeState::InitializingPlatform, "b");
    check(lc.history().size() == 2 && lc.reason() == "b", "lifecycle.history");
  }
  // --- ipc multi-frame stream ---
  {
    auto f1 = sb::ipc::encode(sb::ipc::hello("a"));
    auto f2 = sb::ipc::encode(sb::ipc::hello("b"));
    check(f1.ok() && f2.ok(), "ipc.encode.multi");
    std::vector<std::uint8_t> stream = f1.value();
    stream.insert(stream.end(), f2.value().begin(), f2.value().end());
    std::size_t off = 0;
    int n = 0;
    bool streamOk = true;
    while (off < stream.size()) {
      auto one = sb::ipc::decode(stream.data() + off, stream.size() - off);
      if (!one.ok() || one.value().at("type").asString() != "Hello") {
        streamOk = false;
        break;
      }
      std::uint32_t len = stream[off] | (stream[off + 1] << 8) | (stream[off + 2] << 16) |
                          (stream[off + 3] << 24);
      off += 4 + len;
      ++n;
    }
    check(streamOk && n == 2 && off == stream.size(), "ipc.stream.two-frames");
  }
  // --- config save/load roundtrip ---
  {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "sb-cfg-test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    sb::Settings s = sb::defaults();
    s.resolution = "1920x1080";
    s.frameCap = 120;
    std::string p = (dir / "s.json").string();
    check(sb::saveFile(p, s).ok(), "config.save");
    auto loaded = sb::loadFile(p);
    check(loaded.ok() && loaded.value().settings.resolution == "1920x1080" &&
              loaded.value().settings.frameCap == 120,
          "config.load.roundtrip");
    auto missing = sb::loadFile((dir / "nope.json").string());
    check(missing.ok() && !missing.value().warnings.empty(), "config.missing.defaults");
    std::filesystem::remove_all(dir, ec);
  }
  // --- cache budget ---
  {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "sb-cache-budget";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    sb::cache::ShaderCache tiny(dir.string(), 1000, 10);
    std::vector<std::uint8_t> big(64, 0xAB);
    auto r = tiny.store(sb::cache::entryKey("d3d12", "x", "y"), big);
    check(!r.ok(), "cache.budget.refuses");
    std::filesystem::remove_all(dir, ec);
  }
  // --- host policy patterns (prefix '*' + case-insensitive) ---
  {
    sb::HostPolicy pol;
    pol.allowHostModule("api-ms-win-core-*");
    pol.allowHostModule("Kernel32.dll");
    check(pol.allows("API-MS-WIN-CORE-SYNCH-L1-2-0.DLL"), "hostpolicy.prefix-ci");
    check(pol.allows("kernel32.DLL"), "hostpolicy.exact-ci");
    check(!pol.allows("xmem.dll"), "hostpolicy.deny-unlisted");
    sb::HostPolicy none;
    check(!none.allows("kernel32.dll"), "hostpolicy.default-deny");
  }
  // --- DLL census: HLE wins, then host policy, then missing ---
  {
    sb::ImportRegistry reg;
    reg.registerHle("xgameruntime.dll", "XGameRuntimeInitialize");
    auto ours = reg.censusDll("XGAMERUNTIME.DLL", "XGameRuntimeInitialize");
    check(ours.klass == sb::ImportClass::Ours && ours.provider == "hle:xgameruntime.dll",
          "census-dll.hle-wins");
    sb::HostPolicy pol;
    pol.allowHostModule("api-ms-win-core-*");
    reg.setHostPolicy(pol);
    auto allowed = reg.censusDll("api-ms-win-core-synch-l1-2-0.dll", "Sleep");
    check(allowed.klass == sb::ImportClass::HostAllowed, "census-dll.host-allowed");
    auto denied = reg.censusDll("xmem.dll", "XMemAlloc");
    check(denied.klass == sb::ImportClass::HostDenied, "census-dll.host-denied");
    check(denied.routeKind == "candidate" && denied.resolution == "not_checked",
          "census-dll.honesty-fields");
    // Ordinal symbols are exact strings like "#7".
    reg.registerHle("d3d12_xs.dll", "#7");
    check(reg.censusDll("d3d12_xs.dll", "#7").klass == sb::ImportClass::Ours,
          "census-dll.ordinal");
    check(reg.censusDll("d3d12_xs.dll", "#8").klass == sb::ImportClass::HostDenied,
          "census-dll.ordinal-missing-denied");
  }
  // --- stub database: precedence + call counting ---
  {
    sb::StubDatabase db;
    db.setModuleDefault("xgameruntime.dll", 0x80004001u);
    db.setFunctionReturn("kernelx.dll", "Foo", 7);
    db.setFunctionReturn("*", "SharedFn", 9);
    db.setOrdinalName("d3d12_x.dll", 7, "D3D12CreateDevice");
    check(db.returnFor("kernelx.dll", "Foo") == 7, "stub.exact");
    check(db.returnFor("KERNELX.DLL", "foo") == 7, "stub.case-insensitive");
    check(db.returnFor("other.dll", "SharedFn") == 9, "stub.wildcard");
    check(db.returnFor("xgameruntime.dll", "XSomethingUnknown") == 0x80004001u,
          "stub.module-default");
    check(db.returnFor("xgameruntime.dll", "XUnknown", "xgameruntime.dll") == 0x80004001u,
          "stub.fallback-default");
    check(db.ordinalName("D3D12_X.DLL", 7) == "D3D12CreateDevice", "stub.ordinal-name");
    check(db.ordinalName("d3d12_x.dll", 8).empty(), "stub.ordinal-missing");
    std::uint64_t r1 = db.recordCall("unit.dll", "Answer", 0x1000);
    std::uint64_t r2 = db.recordCall("UNIT.dll", "Answer", 0x2000);
    check(r1 == r2, "stub.return-stable");
    auto snap = db.snapshot();
    bool found = false;
    for (const auto& s : snap)
      if (s.name == "Answer" && s.calls == 2 && s.firstCaller == 0x1000) found = true;
    check(found, "stub.counts-first-caller");
  }
  // --- last-output ring (8-line crash context) ---
  {
    sb::diag::LastOutputRing ring(3);
    ring.push("a");
    ring.push("");
    ring.push("b");
    ring.push("c");
    ring.push("d");
    auto s = ring.snapshot();
    check(s.size() == 3 && s[0] == "b" && s[2] == "d", "lastring.bounded");
  }
  // --- module+RVA fault detail ---
  {
    const void* base = reinterpret_cast<const void*>(0x140000000ull);
    const void* addr = reinterpret_cast<const void*>(0x140001234ull);
    auto fr = sb::diag::formatFaultReportWithBase(0xC0000005, addr, base, "title.exe");
    check(fr.find("title.exe+0x1234") != std::string::npos, "crash.module-rva");
    auto fr2 = sb::diag::formatFaultReportWithBase(0xC0000005, addr, nullptr, "title.exe");
    check(fr2.find("title.exe") != std::string::npos, "crash.fallback-nobase");
  }
  // --- crash log write + policy guard ---
  {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "sb-crash-test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    sb::diag::CrashReport rep;
    rep.id = "CR-1";
    rep.timestamp = "t";
    rep.reason = "boom";
    rep.runtimeVersion = "test";
    check(sb::diag::writeCrashLog((dir / "c.log").string(), rep), "crashlog.write");
    check(std::filesystem::is_regular_file(dir / "c.log", ec), "crashlog.exists");
    std::filesystem::remove_all(dir, ec);
    // Empty path never writes.
    check(!sb::diag::writeCrashLog("", rep), "crashlog.reject-empty");
  }
  // --- derived-data-outside-repo guard ---
  {
    std::error_code ec;
    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "sb-policy-test";
    std::filesystem::remove_all(tmp, ec);
    std::filesystem::create_directories(tmp / "sub", ec);
    check(sb::fs::ensureOutsideRepo((tmp / "sub" / "out.bin").string()).ok(),
          "policy.accept-outside");
    // A tree containing the marker refuses writes beneath it.
    std::filesystem::path marked = tmp / "tree";
    std::filesystem::create_directories(marked, ec);
    {
      std::ofstream f(marked / "SB-ROOT-MARKER.txt");
      f << "marker";
    }
    check(!sb::fs::ensureOutsideRepo((marked / "cache" / "x.bin").string()).ok(),
          "policy.refuse-in-tree");
    // Non-existent tail under a marked tree still refuses.
    check(!sb::fs::ensureOutsideRepo((marked / "not-yet" / "out.bin").string()).ok(),
          "policy.refuse-nonexistent-tail");
    std::filesystem::remove_all(tmp, ec);
  }
  // --- PE import census on a synthetic AMD64 image ---
  {
    // Minimal PE32+ with one section; one normal import (name + ordinal),
    // one RVA-based delay import and one VA-biased delay import.
    std::vector<std::uint8_t> img(0x600, 0);
    auto w16 = [&](std::size_t at, std::uint16_t v) {
      img[at] = static_cast<std::uint8_t>(v);
      img[at + 1] = static_cast<std::uint8_t>(v >> 8);
    };
    auto w32 = [&](std::size_t at, std::uint32_t v) {
      for (int i = 0; i < 4; ++i) img[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
    };
    auto w64 = [&](std::size_t at, std::uint64_t v) {
      for (int i = 0; i < 8; ++i) img[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
    };
    auto wstr = [&](std::size_t at, const char* s) {
      for (std::size_t i = 0; s[i]; ++i) img[at + i] = static_cast<std::uint8_t>(s[i]);
    };
    img[0] = 'M';
    img[1] = 'Z';
    w32(0x3C, 0x80);
    img[0x80] = 'P';
    img[0x81] = 'E';
    w16(0x84, 0x8664);
    w16(0x86, 1);
    w16(0x94, 0xF0);
    const std::size_t opt = 0x98;
    w16(opt, 0x20B);
    w32(opt + 16, 0x1000);
    w64(opt + 24, 0x400000ULL);  // small base: VA-biased delay fields are rva+base
    w32(opt + 108, 16);
    const std::size_t dirs = opt + 112;
    w32(dirs + 1 * 8, 0x2000);
    w32(dirs + 1 * 8 + 4, 40);
    w32(dirs + 13 * 8, 0x2100);
    w32(dirs + 13 * 8 + 4, 96);
    const std::size_t sec = opt + 0xF0;
    wstr(sec, ".rdata");
    w32(sec + 8, 0x300);
    w32(sec + 12, 0x2000);
    w32(sec + 16, 0x400);
    w32(sec + 20, 0x200);
    // Normal import descriptor + tables.
    w32(0x200, 0x2030);
    w32(0x200 + 12, 0x2060);
    w32(0x200 + 16, 0x2040);
    w64(0x230, 0x2070);
    w64(0x230 + 8, 0x8000000000000007ULL);
    w64(0x240, 0x2070);
    w64(0x240 + 8, 0x8000000000000007ULL);
    w16(0x270, 0);
    wstr(0x272, "Sleep");
    wstr(0x260, "kernel32.dll");
    // Delay descriptors: [0] RVA-based, [1] VA-biased, [2] terminator.
    w32(0x300, 1);
    w32(0x300 + 4, 0x2150);
    w32(0x300 + 12, 0x2170);
    w32(0x300 + 16, 0x2180);
    w32(0x320, 0);
    w32(0x320 + 4, 0x4021A0u);  // imageBase + 0x21A0
    w32(0x320 + 12, 0x4021C0u);
    w32(0x320 + 16, 0x4021D0u);
    wstr(0x350, "testdelay.dll");
    w64(0x380, 0x2190);
    w16(0x390, 0);
    wstr(0x392, "DelayedFn");
    wstr(0x3A0, "vabias.dll");
    w64(0x3D0, 0x4021E0ULL);
    w16(0x3E0, 0);
    wstr(0x3E2, "VaFn");
    auto pe = sb::pe::parseImports(img.data(), img.size());
    check(pe.ok(), "pe.parse.synthetic");
    if (pe.ok()) {
      check(pe.value().is64 && pe.value().machine == 0x8664, "pe.headers");
      check(pe.value().imageBase == 0x400000ULL, "pe.imagebase");
      const auto& rows = pe.value().imports;
      bool sleep = false, ord = false, delayed = false, va = false;
      for (const auto& r : rows) {
        if (r.kind == "normal" && r.module == "kernel32.dll" && r.symbol == "Sleep")
          sleep = true;
        if (r.kind == "normal" && r.module == "kernel32.dll" && r.symbol == "#7") ord = true;
        if (r.kind == "delay" && r.module == "testdelay.dll" && r.symbol == "DelayedFn")
          delayed = true;
        if (r.kind == "delay" && r.module == "vabias.dll" && r.symbol == "VaFn") va = true;
      }
      check(sleep && ord && delayed && va, "pe.rows");
      check(rows.size() == 4, "pe.row-count");
    }
    // Unsupported machine fails; garbage fails; truncation fails.
    {
      auto bad = img;
      bad[0x84] = 0x4C;
      bad[0x85] = 0x01;
      check(!sb::pe::parseImports(bad.data(), bad.size()).ok(), "pe.reject-machine");
    }
    {
      auto bad = img;
      bad[0] = 'Z';
      check(!sb::pe::parseImports(bad.data(), bad.size()).ok(), "pe.reject-magic");
    }
    check(!sb::pe::parseImports(img.data(), 10).ok(), "pe.reject-truncated");
  }
  // --- title manifest + image registry ---
  {
    std::string xml =
        "\xEF\xBB\xBF<?xml version=\"1.0\"?>\n<Game configVersion=\"1\">"
        "<!-- <TitleId>00000000</TitleId> -->"
        "<Identity Name=\"x\"/>\n  <TitleId>639AA8DD</TitleId>\n"
        "<![CDATA[ <TitleId>1</TitleId> ]]>                    <StoreId>9&amp;C</StoreId>"
        "<Other><SizeMB>7</SizeMB></Other>"
        "<PersistentLocalStorage>\n  <SizeMB>1024</SizeMB></PersistentLocalStorage></Game>";
    auto m = sb::TitleManifest::parse(xml);
    check(m.titleId == 0x639AA8DDu, "manifest.title-id");
    check(m.storeId == "9&C", "manifest.entities");
    check(m.storageBytes == (1024ULL << 20), "manifest.quota");
    check(sb::TitleManifest::parse("").storageBytes == 0, "manifest.empty");
    sb::TitleManifest withDir;
    withDir.storageDir = "/data/title/storage";
    check(withDir.savesDir() == "/data/title/saves", "manifest.saves-sibling");
    sb::ImageRegistry images;
    images.setMainImage(0x140000000ULL);
    check(images.mainImage() == 0x140000000ULL, "images.main");
    images.setImagePath(0x140000000ULL, "/pkg/game.exe");
    images.setImagePath(0x180000000ULL, "/pkg/DLC.DLL");
    check(!images.imagePath(0x140000000ULL).empty(), "images.path");
    check(images.imageNamed("dlc.dll") == 0x180000000ULL, "images.named-ci");
    images.forgetImage(0x180000000ULL);
    check(images.imageNamed("dlc.dll") == 0, "images.forget");
  }
  // --- fault budget quotas ---
  {
    using sb::diag::FaultBudget;
    check(!FaultBudget::isFault(0xE06D7363u), "fault.customer-excluded");
    check(FaultBudget::isFault(0x80000003u), "fault.breakpoint");
    check(FaultBudget::isFault(0xC0000005u), "fault.access-violation");
    check(!FaultBudget::isFault(0x40000000u), "fault.informational-excluded");
    FaultBudget b(3, 64, 1024);
    bool fullOk = true, briefOk = true;
    for (unsigned i = 0; i < 3; ++i) {
      auto d = b.evaluate(0xC0000005u, true, 0x1000 + i);
      fullOk = fullOk && d.shouldReport && !d.brief;
    }
    auto d4 = b.evaluate(0xC0000005u, true, 0x2000);
    briefOk = d4.shouldReport && d4.brief;
    check(fullOk && briefOk, "fault.full-then-brief");
    FaultBudget q(1000000, 2, 1024);
    q.evaluate(0xC0000005u, true, 0x1);
    q.evaluate(0xC0000005u, true, 0x2);
    auto over = q.evaluate(0xC0000005u, true, 0x2);
    auto fresh = q.evaluate(0xC0000005u, true, 0x3);
    check(!over.shouldReport && fresh.shouldReport, "fault.new-site-despite-quota");
    check(!q.evaluate(0xC0000005u, false, 0x9).shouldReport, "fault.unmapped-excluded");
    check(b.tryEnter() && !b.tryEnter(), "fault.reentrancy-guard");
    b.exit();
    check(b.tryEnter(), "fault.guard-release");
    b.exit();
  }
  // --- refusal census: null causes sum + bounded per-reason table ---
  {
    using sb::diag::BindingFacts;
    using sb::diag::NullCause;
    auto facts = [](bool tex, bool tab, bool read, bool magic, bool known, bool wrap,
                    bool dim, bool bound) {
      BindingFacts f;
      f.isTexture = tex;
      f.fromTable = tab;
      f.entryReadable = read;
      f.tokenMagic = magic;
      f.tokenKnown = known;
      f.resourceIsWrapper = wrap;
      f.dimensionMatches = dim;
      f.rangeBound = bound;
      return f;
    };
    check(sb::diag::classifyNullBinding(facts(false, false, false, false, false, false, true,
                                              true)) == NullCause::None,
          "refuse.bound-not-null");
    check(sb::diag::classifyNullBinding(facts(false, true, false, false, false, false, true,
                                              false)) == NullCause::Entry,
          "refuse.table-entry");
    check(sb::diag::classifyNullBinding(facts(false, true, true, true, false, false, true,
                                              false)) == NullCause::Token,
          "refuse.unknown-token");
    check(sb::diag::classifyNullBinding(facts(true, true, true, false, false, false, true,
                                              false)) == NullCause::Token,
          "refuse.texture-no-magic");
    check(sb::diag::classifyNullBinding(facts(true, true, true, true, true, false, true,
                                              false)) == NullCause::Wrapper,
          "refuse.texture-wrapper");
    check(sb::diag::classifyNullBinding(facts(true, true, true, true, true, true, false,
                                              false)) == NullCause::Dimension,
          "refuse.dimension");
    check(sb::diag::classifyNullBinding(facts(true, true, true, true, true, true, true,
                                              false)) == NullCause::None,
          "refuse.texture-ok");
    check(sb::diag::classifyNullBinding(facts(false, false, false, false, false, false, true,
                                              false)) == NullCause::Buffer,
          "refuse.buffer");
    // Per-cause counts sum to the total by construction.
    sb::diag::NullCensus nc;
    nc.note(NullCause::Entry);
    nc.note(NullCause::Entry);
    nc.note(NullCause::Buffer);
    nc.note(NullCause::None);  // ignored: not a null binding
    check(nc.total() == 3 && nc.counts[0] == 2 && nc.counts[1] == 1, "refuse.census-sum");
    check(std::string(sb::diag::nullCauseName(NullCause::Entry)) == "entry",
          "refuse.cause-name");
    // Per-(key,reason): own counts, 8-detail budget, delta reports, overflow.
    sb::diag::PerReasonCensus pc("ut");
    check(pc.note(1, "missing_guest_root"), "refuse.detail-first");
    check(!pc.note(1, "missing_guest_root"), "refuse.same-no-detail");
    for (unsigned i = 2; i <= 8; ++i) check(pc.note(i, "r"), "refuse.detail-budget");
    check(!pc.note(9, "r9"), "refuse.detail-spent");
    check(sb::diag::PerReasonCensus::sanitizeReason("") == "unknown",
          "refuse.sanitize-empty");
    check(sb::diag::PerReasonCensus::sanitizeReason("a\nb") == "a b",
          "refuse.sanitize-newline");
    check(sb::diag::PerReasonCensus::sanitizeReason(std::string(200, 'x')).size() == 63,
          "refuse.sanitize-cap");
    auto full = pc.report(false);
    check(!full.empty() && full[0].find("total 9") != std::string::npos,
          "refuse.report-full");
    check(pc.report(true).empty(), "refuse.report-delta-quiet");
    pc.note(1, "missing_guest_root");
    check(!pc.report(true).empty(), "refuse.report-delta-grows");
    sb::diag::PerReasonCensus big("big");
    for (unsigned i = 0; i < 128; ++i) big.note(0x1000 + i, "reason");
    check(big.entries() == 128 && big.keys() == 128, "refuse.cap-128");
    check(!big.note(0x9999, "one-more") && big.overflow() == 1 && big.total() == 129,
          "refuse.overflow-counts");
  }
  // --- adapter choice (portable) ---
  {
    using sb::gfx::AdapterInfo;
    std::vector<AdapterInfo> adapters(3);
    adapters[0].description = "NVIDIA GeForce RTX";
    adapters[1].description = "Microsoft Basic Render Driver";
    adapters[1].isSoftware = true;
    adapters[2].description = "Intel UHD";
    check(sb::gfx::chooseAdapter(adapters, "") == 0, "adapter.hardware-first");
    check(sb::gfx::chooseAdapter(adapters, "warp") == 1, "adapter.warp");
    check(sb::gfx::chooseAdapter(adapters, "intel") == 2, "adapter.substring-ci");
    check(sb::gfx::chooseAdapter(adapters, "Geforce rtx") == 0, "adapter.case-insensitive");
    check(sb::gfx::chooseAdapter(adapters, "no-such-gpu") == 1, "adapter.fallback-warp");
    check(sb::gfx::chooseAdapter(adapters, "no-such-gpu", false) == -1,
          "adapter.no-fallback");
    check(sb::gfx::chooseAdapter({}, "") == -1, "adapter.empty");
  }
  // --- adapter selector: auto/warp/index/name + numeric model tokens ---
  {
    using sb::gfx::AdapterInfo;
    using sb::gfx::AdapterSelector;
    auto parse = sb::gfx::parseAdapterSelector;
    check(parse("").kind == AdapterSelector::Auto, "selector.parse-empty");
    check(parse(" Auto ").kind == AdapterSelector::Auto, "selector.parse-auto");
    check(parse("WARP").kind == AdapterSelector::Warp, "selector.parse-warp");
    check(parse("0").kind == AdapterSelector::Index && parse("0").index == 0,
          "selector.parse-zero");
    check(parse("1").kind == AdapterSelector::Index && parse("1").index == 1,
          "selector.parse-one");
    check(parse("007").kind == AdapterSelector::Name && parse("007").name == "007",
          "selector.parse-leading-zero");
    check(parse("+1").kind == AdapterSelector::Name && parse("+1").name == "+1",
          "selector.parse-signed");
    check(parse("Radeon").kind == AdapterSelector::Name &&
              parse("Radeon").name == "radeon",
          "selector.parse-name");
    check(parse(" RTX 3080 ").name == "rtx 3080", "selector.parse-trim");
    check(sb::gfx::containsNumericToken("NVIDIA GeForce RTX 3080", "3080"),
          "selector.token-match");
    check(sb::gfx::containsNumericToken("NVIDIA GeForce RTX 2080", "2080"),
          "selector.token-full");
    check(!sb::gfx::containsNumericToken("NVIDIA GeForce RTX 2080", "2"),
          "selector.token-partial-front");
    check(!sb::gfx::containsNumericToken("NVIDIA GeForce RTX 2080", "208"),
          "selector.token-partial-mid");
    check(!sb::gfx::containsNumericToken("NVIDIA GeForce RTX 2080", "80"),
          "selector.token-partial-back");
    check(!sb::gfx::containsNumericToken("anything", ""), "selector.token-empty");
    std::vector<AdapterInfo> gpus(3);
    gpus[0].description = "NVIDIA GeForce RTX 5070";
    gpus[1].description = "AMD Radeon RX 7900XTX";
    gpus[2].description = "Intel Arc A770 Graphics";
    auto ids = sb::gfx::selectAdapterIndices;
    check(ids(gpus, parse("0")) == std::vector<int>{0}, "selector.index-zero");
    check(ids(gpus, parse("2")) == std::vector<int>{2}, "selector.index-two");
    check(ids(gpus, parse("5070")) == std::vector<int>{0}, "selector.token-5070");
    check(ids(gpus, parse("7900")) == std::vector<int>{1}, "selector.token-7900");
    check(ids(gpus, parse("770")) == std::vector<int>{2}, "selector.token-770");
    check(ids(gpus, parse("9")).empty(), "selector.token-oob-miss");
    check(ids({}, parse("0")).empty(), "selector.index-empty");
    std::vector<AdapterInfo> mixed(3);
    mixed[0].description = "NVIDIA GeForce RTX";
    mixed[1].description = "Microsoft Basic Render Driver";
    mixed[1].isSoftware = true;
    mixed[2].description = "Intel UHD";
    check(sb::gfx::chooseAdapter(mixed, "1") == 2, "selector.choose-index");
    check(sb::gfx::chooseAdapter(mixed, "auto") == 0, "selector.choose-auto-word");
    check(sb::gfx::chooseAdapter(mixed, "5") == 1, "selector.choose-oob-fallback");
  }
  // --- adapter selector wording for the settings UI (never throws) ---
  {
    auto describe = sb::gfx::describeSelector;
    auto parse = sb::gfx::parseAdapterSelector;
    check(describe(parse("")).find("first hardware") != std::string::npos,
          "selector.describe-auto");
    check(describe(parse("warp")).find("WARP") != std::string::npos,
          "selector.describe-warp");
    check(describe(parse("2")).find("2") != std::string::npos &&
              describe(parse("2")).find("model token") != std::string::npos,
          "selector.describe-index");
    check(describe(parse("RTX")).find("rtx") != std::string::npos &&
              describe(parse("RTX")).find("case-insensitive") != std::string::npos,
          "selector.describe-name");
    check(describe(parse("5070")).find("5070") != std::string::npos,
          "selector.describe-index-token");
  }
  // --- scripted input: parse + playback (waits/marks/groups unsupported) ---
  {
    using sb::input::InputScript;
    auto parse = sb::input::parseInputScript;
    struct RejectCase {
      const char* script;
      std::size_t accepted, index;
      const char* reason;
    };
    const RejectCase rejectCases[] = {
        {"4", 0, 1, "missing @<seconds>"},
        {"4 @1", 0, 1, "missing @<seconds>"},
        {"4@", 0, 1, "bad time"},
        {"4@x", 0, 1, "bad time"},
        {"4@1+0", 0, 1, "bad duration"},
        {"4@1+-1", 0, 1, "bad duration"},
        {"4@1+", 0, 1, "bad duration"},
        {"4@1junk", 0, 1, "trailing text"},
        {"4@1,zz@2,8@3", 2, 2, "bad mask"},
        {"1FFFFFFFF@1", 0, 1, "bad mask"},
        {"lx=2@1", 0, 1, "value out of range [-1, 1]"},
        {"lt=-0.5@1", 0, 1, "value out of range [0, 1]"},
        {"qq=1@1", 0, 1, "unknown token"},
        {"lx=@1", 0, 1, "bad value"},
        {"ly=0.5", 0, 1, "missing @<seconds>"},
        {"4@1,base=start", 1, 2, "base= must be the first item"},
        {"base=later,4@1", 1, 1, "unknown base (use poll or start)"},
        {"2@1e999", 0, 1, "bad time"},
        {"1@+1e308,2@+1e308", 1, 2, "bad time"},
        {"1@1e308,2@+1e308", 1, 2, "bad time"},
    };
    for (const RejectCase& c : rejectCases) {
      InputScript s = parse(c.script);
      check(s.entries.size() == c.accepted && s.rejections.size() == 1 &&
                s.rejections[0].index == c.index &&
                s.rejections[0].reason == c.reason,
            "script.reject");
    }
    {
      InputScript based = parse(" base = start ,4@1,lx=-1@2+0.5,rt=1@3");
      check(based.fromProcessStart && based.entries.size() == 3 &&
                based.rejections.empty(),
            "script.base-start");
      check(based.entries[1].axis == 2 && based.entries[1].value == -1.0f &&
                based.entries[1].duration == 0.5,
            "script.axis-entry");
    }
    {
      std::string text;
      for (int i = 0; i < 257; ++i) text += (i ? ",1@" : "1@") + std::to_string(i);
      InputScript full = parse(text);
      check(full.entries.size() == 256 && full.rejections.size() == 1 &&
                full.rejections[0].index == 257 &&
                full.rejections[0].reason == "over 256 entries" && full.items == 257,
            "script.256-cap");
    }
    {
      InputScript s = parse("4@1,1@70,20@80");
      check(s.entries.size() == 3 && s.rejections.empty(), "script.legacy-shape");
      check(sb::input::playInputScript(s, 1.15).buttons == 0x4, "script.hold");
      check(sb::input::playInputScript(s, 70.2).buttons == 0x1, "script.second");
      check(sb::input::playInputScript(s, 0.9).buttons == 0, "script.before");
    }
    {
      InputScript s = parse("4@1,8@1.1");
      check(sb::input::playInputScript(s, 1.2).buttons == 0xC, "script.overlap-or");
      check(sb::input::playInputScript(s, 1.35).buttons == 0x8, "script.first-ends");
      InputScript rel = parse("4@1,8@+1");
      check(rel.entries.size() == 2 && rel.entries[1].at == 2.0, "script.relative");
    }
    {
      InputScript s = parse("lx=-1@1,lx=1@1,lt=0.5@1");
      auto st = sb::input::playInputScript(s, 1.1);
      check(st.axes[2] == 1.0f && st.active[2], "script.later-axis-wins");
      check(st.axes[0] == 0.5f && st.active[0], "script.trigger");
      check(!st.active[3], "script.inactive-axis");
    }
    {
      InputScript s = parse("200@0,400@0.3,800@0.6");
      check(sb::input::firstInputStart(s) == 0.0, "script.first-start");
      const double last = sb::input::lastInputEnd(s);
      check(last > 0.89 && last < 0.91, "script.last-end");
      check(sb::input::firstInputStart(InputScript()) == 0.0 &&
                sb::input::lastInputEnd(InputScript()) == 0.0,
            "script.empty-times");
    }
    {
      InputScript s = parse("4@1,,8@2,");
      check(s.entries.size() == 2 && s.items == 2 && s.rejections.empty(),
            "script.blanks-skipped");
      InputScript w = parse("4@1,wait,8@3");
      check(w.entries.size() == 2 && w.rejections.size() == 1 &&
                w.rejections[0].reason ==
                    "unsupported directive (waits, marks and groups need a presenter)",
            "script.wait-unsupported");
    }
  }
  // --- HLE coverage table: case-insensitive DLL, exact symbols, no dupes ---
  {
    sb::ImportRegistry reg;
    reg.registerHle("XGameRuntime.dll", "XGameRuntimeInitialize");
    reg.registerHle("xgameruntime.dll", "XGameRuntimeInitialize");
    check(reg.hasHle("XGAMERUNTIME.DLL", "XGameRuntimeInitialize"), "hle.ci-dll");
    check(!reg.hasHle("xgameruntime.dll", "xgameruntimeinitialize"), "hle.symbol-exact");
    auto e = reg.censusDll("xgameruntime.dll", "XGameRuntimeInitialize");
    check(e.klass == sb::ImportClass::Ours, "hle.census-ours");
    check(reg.findHleProvider("XGameRuntimeInitialize").has_value(),
          "hle.provider-found");
    check(!reg.findHleProvider("#7").has_value(), "hle.provider-ordinal-never");
    check(!reg.findHleProvider("NoSuchApi").has_value(), "hle.provider-missing");
    check(reg.hasAnyHle("XGAMERUNTIME.dll"), "hle.any-dll");
    check(!reg.hasAnyHle("other.dll"), "hle.any-missing");
  }
  // --- dry-run resolver: Source order + ImportRecord log + OS contracts ---
  {
    sb::ImportRegistry reg;
    sb::HostPolicy pol;
    pol.allowHostModule("kernel32.dll");
    pol.allowHostModule("api-ms-win-core-*");
    reg.setHostPolicy(pol);
    reg.registerHle("gameinput.dll", "GameInputCreate");
    reg.registerHle("kernelx.dll", "RegOpenKeyW");
    sb::ImportResolver rx(&reg);
    rx.setSearchDirs({"/pkg/game"});
    rx.setGameFiles({"/pkg/game/shipped.dll", "/pkg/game/XGameRuntime.dll"});
    rx.setSystemExports({"kernel32.dll!GetProcessId", "api-ms-win-core-sys.dll!Beep"});
    // HLE exact wins over everything.
    check(rx.classify("gameinput.dll", "GameInputCreate") == sb::ResolvedSource::Hle,
          "resolve.hle-exact");
    // Game DLL beats system even when the policy would allow the name.
    check(rx.classify("SHIPPED.DLL", "Anything") == sb::ResolvedSource::GameDll,
          "resolve.gamedll-ci");
    // System needs policy + export present.
    check(rx.classify("kernel32.dll", "GetProcessId") == sb::ResolvedSource::System,
          "resolve.system");
    check(rx.classify("kernel32.dll", "NoSuchExport") == sb::ResolvedSource::Stub,
          "resolve.system-needs-export");
    check(rx.classify("other.dll", "Nope") == sb::ResolvedSource::Stub, "resolve.stub");
    // Named cross-DLL lookup finds the kernelx provider...
    check(rx.classify("advapi32.dll", "RegOpenKeyW") == sb::ResolvedSource::HleOther,
          "resolve.hle-other");
    // ...but raw ordinals never resolve cross-DLL.
    reg.registerHle("kernelx.dll", "#7");
    check(rx.classify("advapi32.dll", "#7") == sb::ResolvedSource::Stub,
          "resolve.ordinal-no-cross");
    // Record log carries importer + delay flag.
    rx.clearRecords();
    rx.resolveImport("game.exe", "kernel32.dll", "GetProcessId", false);
    rx.resolveImport("game.exe", "advapi32.dll", "RegOpenKeyW", true);
    check(rx.records().size() == 2 && rx.records()[1].delay &&
              rx.records()[1].source == sb::ResolvedSource::HleOther,
          "resolve.records");
    // GetProcAddress: missing is null, never a stub.
    check(!rx.getProcAddress("kernel32.dll", "NoSuchExport").has_value(),
          "resolve.getproc-null");
    check(rx.getProcAddress("gameinput.dll", "GameInputCreate").has_value(),
          "resolve.getproc-hle");
    check(rx.getProcAddress("advapi32.dll", "RegOpenKeyW").has_value(),
          "resolve.getproc-hle-other");
    check(!rx.getProcAddress("advapi32.dll", "#7").has_value(),
          "resolve.getproc-ordinal-null");
    // LoadLibrary kinds.
    check(rx.loadLibrary("gameinput.dll").value_or("?") == "hle", "resolve.load-hle");
    check(rx.loadLibrary("shipped.dll").value_or("?") == "gamedll", "resolve.load-game");
    check(rx.loadLibrary("kernel32.dll").value_or("?") == "system", "resolve.load-system");
    check(!rx.loadLibrary("not-here.dll").has_value(), "resolve.load-null");
    // findGameDll matches by file name only (absolute or bare).
    check(rx.findGameDll("C:\\pkg\\SHIPPED.dll").value_or("?") == "/pkg/game/shipped.dll",
          "resolve.findgame-ci");
  }
  // --- host paths: System32 only, planting refused ---
  {
    check(sb::system32PathFor("kernel32.dll") == "C:\\Windows\\System32\\kernel32.dll",
          "hostpath.full");
    check(sb::system32PathFor("evil\\kernel32.dll") ==
              "C:\\Windows\\System32\\kernel32.dll",
          "hostpath.strips-dirs");
    check(sb::isSystem32Path("C:\\Windows\\System32\\kernel32.dll"), "hostpath.is-system");
    check(sb::isSystem32Path("c:/windows/system32/version.dll"), "hostpath.slash-ci");
    check(!sb::isSystem32Path("C:\\game\\kernel32.dll"), "hostpath.reject-cwd");
    check(sb::isPlantingAttempt("C:\\game\\kernel32.dll", "kernel32.dll"),
          "hostpath.planting");
    check(!sb::isPlantingAttempt("C:\\Windows\\System32\\kernel32.dll", "kernel32.dll"),
          "hostpath.no-planting");
  }
  // --- console timers: 1 ms request, opt-out flag ---
  {
    check(sb::kConsoleTimerResolutionMs == 1, "timer.resolution-1ms");
    check(sb::clampTimerRequestMs(0) == 1, "timer.clamp-low");
    check(sb::clampTimerRequestMs(99) == 16, "timer.clamp-high");
    check(sb::efficiencyOptOut(nullptr), "timer.optout-default");
    check(!sb::efficiencyOptOut("1"), "timer.optout-env1");
    check(sb::consoleTimerPolicy(nullptr).keepEfficiencyOff, "timer.policy");
  }
  // --- trace throttle + filter: first-only, bounded, stub-silent ---
  {
    using sb::diag::FirstSeenThrottle;
    FirstSeenThrottle th(2);
    check(th.first("p", "i") && !th.first("p", "i"), "trace.first-only");
    check(th.first("p2", "i2"), "trace.second");
    check(!th.first("p3", "i3"), "trace.table-full-silent");
    using sb::diag::TraceFilter;
    using sb::diag::TraceSource;
    auto def = TraceFilter::parse("");
    check(def.wants("gameinput.dll", "GameInputCreate", TraceSource::Hle),
          "tracefilter.hle-default");
    check(!def.wants("kernel32.dll", "Beep", TraceSource::System),
          "tracefilter.system-off-default");
    check(!def.wants("x.dll", "F", TraceSource::Stub), "tracefilter.stub-never");
    auto f = TraceFilter::parse("all,-kernel32!beep,gameinput");
    check(f.wants("kernel32.dll", "Sleep", TraceSource::System), "tracefilter.all");
    check(!f.wants("kernel32.dll", "Beep", TraceSource::System),
          "tracefilter.exclude");
    check(f.wants("gameinput.dll", "Anything", TraceSource::GameDll),
          "tracefilter.include-prefix");
    auto g = TraceFilter::parse("kernelx!gettick");
    check(g.wants("kernelx.dll", "GetTickCount", TraceSource::Hle),
          "tracefilter.hle-still");
    check(!g.wants("other.dll", "Other", TraceSource::System),
          "tracefilter.no-match");
  }
  // --- shader cache key v2 + warm/get-or-translate ---
  {
    using sb::cache::CacheCounters;
    using sb::cache::ShaderCache;
    std::string fp = sb::cache::translatorFingerprint();
    check(!fp.empty(), "cachev2.fingerprint");
    std::string k1 = sb::cache::cacheKeyV2(fp, "d3d12", "aaa");
    std::string k2 = sb::cache::cacheKeyV2(fp, "d3d12", "bbb");
    check(k1.size() == 64 && k1 != k2, "cachev2.container-matters");
    check(sb::cache::cacheKeyV2(fp, "d3d12", "aaa") !=
              sb::cache::cacheKeyV2(fp, "warp", "aaa"),
          "cachev2.backend-matters");
    auto dir =
        (std::filesystem::temp_directory_path() / "sb-cachev2-test").string();
    ShaderCache cache(dir, 64, 1u << 20);
    (void)cache.clear();
    CacheCounters c;
    auto key = sb::cache::cacheKeyV2(fp, "d3d12-test", "container1");
    auto miss = cache.getOrTranslate(
        key, [] { return sb::Result<std::vector<std::uint8_t>>::success(std::vector<std::uint8_t>{1, 2, 3}); }, c);
    check(miss.ok() && c.misses == 1, "cachev2.miss-translates");
    auto hit = cache.getOrTranslate(
        key, [] {
          return sb::Result<std::vector<std::uint8_t>>::fail(
              sb::Error::make("t", 1, "must not run on hit"));
        },
        c);
    check(hit.ok() && hit.value() == std::vector<std::uint8_t>({1, 2, 3}) && c.hits == 1,
          "cachev2.hit");
    (void)cache.clear();
  }
  // --- forwarder specs: last-dot split, #ordinals, depth cap ---
  {
    auto t = sb::parseForwarderSpec("dotted.fwdtarget.RealExport");
    check(t.has_value() && t->dllKey == "dotted.fwdtarget.dll" && !t->isOrdinal &&
              t->name == "RealExport",
          "fwd.dotted");
    auto k = sb::parseForwarderSpec("KERNEL32.GetCurrentProcessId");
    check(k.has_value() && k->dllKey == "kernel32.dll", "fwd.dll-suffix");
    auto o = sb::parseForwarderSpec("mod.#17");
    check(o.has_value() && o->isOrdinal && o->ordinal == 17, "fwd.ordinal");
    check(!sb::parseForwarderSpec("nodot").has_value(), "fwd.reject-nodot");
    check(!sb::parseForwarderSpec(".Name").has_value(), "fwd.reject-empty-dll");
    check(!sb::parseForwarderSpec("dll.").has_value(), "fwd.reject-empty-func");
    check(!sb::parseForwarderSpec("dll.#x").has_value(), "fwd.reject-bad-ord");
    // Chain: proxy -> dotted target resolves; missing stays missing (null);
    // self-loop hits the depth cap.
    std::map<std::string, sb::ExportOutcome> db;
    db["fwd_proxy_unit.dll!dottedfwd"] = sb::ExportOutcome{true, "dotted.fwdtarget.RealExport", 0};
    db["dotted.fwdtarget.dll!realexport"] =
        sb::ExportOutcome{false, "", 0x1234};  // note: lookup is exact-symbol
    auto lookup = [&](const std::string& dll, const std::string& sym)
        -> std::optional<sb::ExportOutcome> {
      std::string s = sym;
      auto it = db.find(dll + "!" + s);
      if (it != db.end()) return it->second;
      // case-insensitive fallback for the test db only
      for (auto& kv : db) {
        std::string a = kv.first, b = dll + "!" + s;
        for (char& c : a) c = static_cast<char>(std::tolower((unsigned char)c));
        for (char& c : b) c = static_cast<char>(std::tolower((unsigned char)c));
        if (a == b) return kv.second;
      }
      return std::nullopt;
    };
    auto hit = sb::resolveForwarderChain("fwd_proxy_unit.DottedFwd", lookup);
    check(hit.has_value() && hit->rva == 0x1234 &&
              hit->dllKey == "dotted.fwdtarget.dll",
          "fwd.chain-dotted");
    check(!sb::resolveForwarderChain("fwd_proxy_unit.MissingFwd", lookup).has_value(),
          "fwd.missing-null");
    db["loop.dll!a"] = sb::ExportOutcome{true, "loop.B", 0};
    db["loop.dll!b"] = sb::ExportOutcome{true, "loop.A", 0};
    check(!sb::resolveForwarderChain("loop.A", lookup).has_value(), "fwd.loop-depth-cap");
  }
  // --- image layout: containment, protections, relocs, TLS bounds ---
  {
    using sb::layout::Protection;
    check(sb::layout::sectionProtection(0x60000020u) == Protection::ExecuteRead,
          "layout.prot-xr");
    check(sb::layout::sectionProtection(0xE0000020u) == Protection::ExecuteReadWrite,
          "layout.prot-xrw");
    check(sb::layout::sectionProtection(0x40000040u) == Protection::ReadOnly,
          "layout.prot-r");
    check(sb::layout::sectionProtection(0x80000040u) == Protection::ReadWrite,
          "layout.prot-rw");
    check(sb::layout::sectionProtection(0x20000000u) == Protection::Execute,
          "layout.prot-x");
    check(sb::layout::sectionProtection(0u) == Protection::NoAccess, "layout.prot-none");
    sb::pe::PeSection text, data;
    text.name = ".text";
    text.rva = 0x1000;
    text.virtualSize = 0x200;
    text.rawSize = 0x200;
    text.characteristics = 0x60000020u;
    data.name = ".data";
    data.rva = 0x2000;
    data.virtualSize = 0x100;
    data.rawSize = 0x100;
    data.characteristics = 0xC0000040u;
    std::vector<sb::pe::PeSection> secs{text, data};
    check(sb::layout::isExecutableRva(0x1010, secs), "layout.exec-text");
    check(!sb::layout::isExecutableRva(0x2010, secs), "layout.exec-data-no");
    check(sb::layout::isWritableRva(0x2010, 8, secs), "layout.write-data");
    check(!sb::layout::isWritableRva(0x1010, 8, secs), "layout.write-text-no");
    check(!sb::layout::isWritableRva(0x20F0, 0x20, secs), "layout.write-span");
    check(sb::layout::rvaInsideImage(0x100, 0x10, 0x1000), "layout.inside");
    check(!sb::layout::rvaInsideImage(0xFFF, 0x10, 0x1000), "layout.outside");
    check(!sb::layout::rvaInsideImage(0xFFFFFFFFULL, 0x10, 0x1000),
          "layout.overflow-safe");
    check(sb::layout::isValidEntryPoint(0x1010, 0x3000, secs), "layout.entry-ok");
    check(!sb::layout::isValidEntryPoint(0, 0x3000, secs), "layout.entry-zero");
    check(!sb::layout::isValidEntryPoint(0x2010, 0x3000, secs),
          "layout.entry-data-no");
    check(sb::layout::isSupportedReloc(sb::layout::kRelocDir64), "layout.reloc64");
    check(sb::layout::isSupportedReloc(sb::layout::kRelocHighLow), "layout.relochl");
    check(!sb::layout::isSupportedReloc(7), "layout.reloc-unsupported");
    {
      std::uint64_t out = 0;
      check(sb::layout::applyReloc(0x1000, sb::layout::kRelocDir64, 0x5000, out) &&
                out == 0x6000,
            "layout.reloc64-apply");
    }
    {
      std::uint64_t out = 0;
      check(sb::layout::applyReloc(0xFFFFFFFFULL, sb::layout::kRelocHighLow, 2, out) &&
                out == 1,
            "layout.relochl-wrap");
    }
    {
      std::uint64_t out = 0;
      check(!sb::layout::applyReloc(0x1000, 7, 1, out), "layout.reloc-warn");
    }
    check(sb::layout::validateTlsCallbacks({0x1010, 0x1020}, 0x3000),
          "layout.tls-ok");
    check(sb::layout::validateTlsCallbacks({}, 0x3000), "layout.tls-empty-ok");
    check(!sb::layout::validateTlsCallbacks({0x5000}, 0x3000), "layout.tls-outside");
    std::vector<std::uint32_t> many(1025, 0x1000);
    check(!sb::layout::validateTlsCallbacks(many, 0x10000), "layout.tls-cap");
  }
  // --- PE export directory on a synthetic image ---
  {
    // Headers + one section; export dir (index 0) at RVA 0x2000 (file 0x200),
    // size 0x80. Two names (ApiOne->code, ApiTwo->forwarder), one ordinal-only
    // code export, one zero gap skipped.
    std::vector<std::uint8_t> img(0x600, 0);
    auto w16 = [&](std::size_t at, std::uint16_t v) {
      img[at] = static_cast<std::uint8_t>(v);
      img[at + 1] = static_cast<std::uint8_t>(v >> 8);
    };
    auto w32 = [&](std::size_t at, std::uint32_t v) {
      for (int i = 0; i < 4; ++i) img[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
    };
    auto wstr = [&](std::size_t at, const char* s) {
      for (std::size_t i = 0; s[i]; ++i) img[at + i] = static_cast<std::uint8_t>(s[i]);
    };
    img[0] = 'M';
    img[1] = 'Z';
    w32(0x3C, 0x80);
    img[0x80] = 'P';
    img[0x81] = 'E';
    w16(0x84, 0x8664);
    w16(0x86, 1);
    w16(0x94, 0xF0);
    const std::size_t opt = 0x98;
    w16(opt, 0x20B);
    w32(opt + 16, 0x1000);
    for (int i = 0; i < 8; ++i)
      img[opt + 24 + i] = static_cast<std::uint8_t>(0x400000ULL >> (8 * i));
    w32(opt + 108, 16);
    const std::size_t dirs = opt + 112;
    w32(dirs + 0 * 8, 0x2000);
    w32(dirs + 0 * 8 + 4, 0x80);
    const std::size_t sec = opt + 0xF0;
    wstr(sec, ".rdata");
    w32(sec + 8, 0x400);
    w32(sec + 12, 0x2000);
    w32(sec + 16, 0x400);
    w32(sec + 20, 0x200);
    // Export directory (40 bytes at file 0x200 = RVA 0x2000).
    w32(0x200 + 16, 1);      // Base
    w32(0x200 + 20, 4);      // NumberOfFunctions
    w32(0x200 + 24, 2);      // NumberOfNames
    w32(0x200 + 28, 0x2028);  // AddressOfFunctions
    w32(0x200 + 32, 0x2038);  // AddressOfNames
    w32(0x200 + 36, 0x2040);  // AddressOfNameOrdinals
    // Functions: [code, forwarder-inside-dir, gap, ordinal-only code].
    w32(0x228, 0x3000);
    w32(0x228 + 4, 0x2060);  // inside [0x2000,0x2080): forwarder string
    w32(0x228 + 8, 0);
    w32(0x228 + 12, 0x3010);
    // Names + ordinals: ApiOne->0, ApiTwo->1.
    w32(0x238, 0x2050);
    w32(0x238 + 4, 0x2058);
    w16(0x240, 0);
    w16(0x240 + 2, 1);
    wstr(0x250, "ApiOne");
    wstr(0x258, "ApiTwo");
    wstr(0x260, "other.dll.ApiX");
    auto ex = sb::pe::parseExports(img.data(), img.size());
    check(ex.ok(), "exports.parse");
    if (ex.ok()) {
      check(ex.value().features.hasExports, "exports.feature");
      const auto& rows = ex.value().exports;
      bool one = false, two = false, ordOnly = false;
      for (const auto& r : rows) {
        if (r.name.has_value() && *r.name == "ApiOne" && r.rva.has_value() &&
            *r.rva == 0x3000)
          one = true;
        if (r.name.has_value() && *r.name == "ApiTwo" && r.forwarder.has_value() &&
            *r.forwarder == "other.dll.ApiX")
          two = true;
        if (!r.name.has_value() && r.ordinal == 4 && r.rva.has_value() &&
            *r.rva == 0x3010)
          ordOnly = true;
      }
      check(one && two && ordOnly, "exports.rows");
      check(rows.size() == 3, "exports.row-count");
      auto sorted = rows;
      std::sort(sorted.begin(), sorted.end(), sb::pe::exportLess);
      check(!sorted.front().name.has_value(), "exports.sort-ord-first");
    }
    {
      auto bad = img;
      bad[0x84] = 0x4C;
      bad[0x85] = 0x01;
      check(!sb::pe::parseExports(bad.data(), bad.size()).ok(), "exports.reject-machine");
    }
    check(!sb::pe::parseExports(img.data(), 10).ok(), "exports.reject-truncated");
  }
  // --- route table: canonical defaults, aliases first, glob, find-any ---
  {
    sb::RouteTable t = sb::defaultRoutes();
    check(t.hleFor("kernelx.dll").value_or("?") == "kernelx.dll", "routes.kernelx");
    check(t.hleFor("KERNEL32.dll").value_or("?") == "kernelx.dll", "routes.kernel-ci");
    check(t.hleFor("api-ms-win-core-file-l1-1-0.dll").value_or("?") == "kernelx.dll",
          "routes.apiset");
    check(t.hleFor("xgameruntime.dll").value_or("?") == "xgameruntime.dll",
          "routes.xgrt");
    check(t.hleFor("d3d12_xs.dll").value_or("?") == "d3d12_xs.dll", "routes.d3d12");
    check(t.hleFor("xaudio2_9.dll").value_or("?") == "xaudio2_9.dll", "routes.xaudio");
    check(t.hleFor("mfplat.dll").value_or("?") == "mfplat.dll", "routes.mfplat");
    check(!t.hleFor("ucrtbase.dll").has_value(), "routes.ucrt-none");
    check(!t.hleFor("api-ms-win-crt-runtime-l1-1-0.dll").has_value(),
          "routes.crt-none");
    check(sb::globMatch("xgameruntime*.dll", "XGAMERUNTIME_THUNKS.DLL"),
          "routes.glob-star");
    check(sb::globMatch("api-ms-win-core-*", "api-ms-win-core-synch-l1-1-0.dll"),
          "routes.glob-prefix");
    check(!sb::globMatch("api-ms-win-core-*", "api-ms-win-crt-runtime-l1-1-0.dll"),
          "routes.glob-no");
    // User aliases take precedence over defaults.
    sb::RouteTable a = sb::defaultRoutes();
    a.registerModule("xgameruntime.dll", {"XGameRuntimeInitialize"});
    a.addAlias("kernel32.dll", "@system");
    a.addAlias("mystery*.dll", "@stub");
    a.addAlias("xboxthing.dll", "xgameruntime.dll");
    auto rk = a.route("kernel32.dll");
    check(rk.has_value() && rk->kind == sb::RouteKind::System, "routes.alias-system");
    auto rs = a.route("mystery42.dll");
    check(rs.has_value() && rs->kind == sb::RouteKind::Stub, "routes.alias-stub");
    check(a.hleFor("xboxthing.dll").value_or("?") == "xgameruntime.dll",
          "routes.alias-hle");
    check(a.hleFor("kernel32.dll").has_value() == false, "routes.alias-overrides");
    // Unknown alias targets are skipped (table unchanged).
    auto before = a.route("nope.dll");
    a.addAlias("nope.dll", "nosuchmodule.dll");
    check(a.route("nope.dll").has_value() == before.has_value(), "routes.alias-unknown-skip");
    // Cross-module lookup + ordinal refusal + per-module uniqueness.
    sb::RouteTable r = sb::defaultRoutes();
    check(r.registerModule("kernelx.dll", {"XMemAlloc", "RegOpenKeyW"}),
          "routes.reg-ok");
    check(r.findAny("XMemAlloc").value_or("?") == "kernelx.dll", "routes.find-any");
    check(!r.findAny("#7").has_value(), "routes.find-ordinal-never");
    check(!r.findAny("NoSuchApi").has_value(), "routes.find-missing");
    check(!r.registerModule("kernelx.dll", {"A", "A"}), "routes.dup-rejected");
    sb::RouteTable api;
    check(api.registerModule("t", {}), "routes.reg-empty");
    (void)api;
  }
  // --- init order: deps-first, cycle-safe + run-mode guard + bindings ---
  {
    using sb::ModuleNode;
    std::vector<ModuleNode> mapped = {
        {"game.exe", {"shipped.dll", "kernel32.dll"}},
        {"shipped.dll", {"helper.dll"}},
        {"helper.dll", {}},
    };
    auto order = sb::computeInitOrder(mapped);
    check(order.size() == 3 && order[0] == "helper.dll" && order[1] == "shipped.dll" &&
              order[2] == "game.exe",
          "init.deps-first");
    // Host/shim imports are ignored; matching is case-insensitive.
    std::vector<ModuleNode> mixed = {{"A.DLL", {"kernel32.dll", "B.dll"}}, {"b.dll", {}}};
    auto o2 = sb::computeInitOrder(mixed);
    check(o2.size() == 2 && o2[0] == "b.dll", "init.ci-host-ignored");
    // Cycle is broken at the closer: every module appears exactly once.
    std::vector<ModuleNode> cyc = {{"a.dll", {"b.dll"}}, {"b.dll", {"a.dll"}}};
    auto o3 = sb::computeInitOrder(cyc);
    check(o3.size() == 2, "init.cycle-once");
    // Exactly-one-mode guard.
    check(sb::parseRunMode(true, false, false).value_or(sb::RunMode::Init) ==
              sb::RunMode::DryRun,
          "runmode.dry");
    check(sb::parseRunMode(false, false, true).value_or(sb::RunMode::DryRun) ==
              sb::RunMode::Run,
          "runmode.run");
    check(!sb::parseRunMode(false, false, false).has_value(), "runmode.none");
    check(!sb::parseRunMode(true, false, true).has_value(), "runmode.two");
    // Binding counts from resolver sources.
    using sb::ResolvedSource;
    sb::BindingCounts bc = sb::countBindings({ResolvedSource::Hle,
                                             ResolvedSource::HleOther,
                                             ResolvedSource::System,
                                             ResolvedSource::GameDll,
                                             ResolvedSource::Stub});
    check(bc.shim == 2 && bc.host == 1 && bc.module == 1 && bc.unimplemented == 1 &&
              bc.total() == 5,
          "bindings.counts");
    check(sb::toString(sb::Binding::Shim) == "shim", "bindings.name");
  }
  // --- ui support: versions + read-only capped folder scan + summaries ---
  {
    sb::ui::Versions v = sb::ui::versions();
    check(!v.app.empty() && !v.build.empty(), "ui.versions-present");
    check(v.ipc == "1" && v.schema == "3", "ui.versions-ipc-schema");
    check(!sb::ui::cacheFingerprint().empty(), "ui.fingerprint");
    check(!sb::ui::systemLine().empty(), "ui.system-line");
    check(!sb::ui::gamepadLine().empty(), "ui.gamepad-line");
    check(!sb::ui::audioLine().empty(), "ui.audio-line");
    check(!sb::ui::adapterLine().empty(), "ui.adapter-line");
    // Missing dir fails, never throws.
    auto miss = sb::ui::scanFolderReadOnly("/sb-definitely-missing-dir-xyz", false);
    check(!miss.ok, "ui.scan-missing");
    // Temp dir with two small files scans read-only.
    namespace fs = std::filesystem;
    auto dir = (fs::temp_directory_path() / "sb-ui-scan-test").string();
    std::error_code ec;
    fs::create_directories(dir, ec);
    {
      std::ofstream f(fs::path(dir) / "a.txt", std::ios::binary);
      f << "hello";
    }
    {
      std::ofstream f(fs::path(dir) / "b.bin", std::ios::binary);
      f << "world!";
    }
    auto hit = sb::ui::scanFolderReadOnly(dir, false);
    check(hit.ok && hit.entries >= 2 && hit.shownDir == dir, "ui.scan-ok");
    auto red = sb::ui::scanFolderReadOnly(dir, true);
    check(red.ok && red.shownDir == "<redacted>", "ui.scan-redact");
    fs::remove_all(dir, ec);
  }
  // --- ui support: full settings option lists + validators + summary ---
  {
    auto be = sb::ui::settingBackends();
    auto wm = sb::ui::settingWindowModes();
    auto rs = sb::ui::settingResolutions();
    check(be.size() == 1 && be[0] == "d3d12", "ui.backends");
    check(wm.size() == 3, "ui.windowmodes");
    check(rs.size() == 4, "ui.resolutions");
    check(sb::ui::isValidBackend("d3d12") && !sb::ui::isValidBackend("vulkan"),
          "ui.valid-backend");
    check(sb::ui::isValidWindowMode("borderless") && !sb::ui::isValidWindowMode("popout"),
          "ui.valid-mode");
    check(sb::ui::isValidResolution("1920x1080") && !sb::ui::isValidResolution("9999x9999"),
          "ui.valid-res");
    sb::Settings d = sb::defaults();
    std::string sum = sb::ui::settingsSummary(d);
    check(sum.find("d3d12") != std::string::npos && sum.find("1280x720") != std::string::npos,
          "ui.summary-defaults");
    // Round-trip through the validator: an illegal resolution is corrected and warned.
    sb::json::Value v = sb::toJson(d);
    v.set("resolution", sb::json::Value::str("9999x9999"));
    sb::SettingsLoad fixed = sb::fromJson(v);
    check(fixed.settings.resolution == "1280x720" && !fixed.warnings.empty(),
          "ui.summary-corrects");
  }
  // --- ui support: diagnostics/logs filter + redacted guarded export ---
  {
    check(sb::ui::matchesFilter("anything", ""), "ui.filter-empty");
    check(sb::ui::matchesFilter("Live Diagnostics", "diagnostics"), "ui.filter-ci");
    check(!sb::ui::matchesFilter("graphics", "audio"), "ui.filter-no");
    std::string red = sb::ui::redactLine("open C:\\Users\\bob\\file.txt ok");
    check(red.find("bob") == std::string::npos && red.find("<redacted>") != std::string::npos,
          "ui.redact-drive");
    std::vector<std::pair<std::string, double>> counters = {{"synthetic.tick", 3}};
    sb::diag::TraceEvent e;
    e.t = "t";
    e.subsystem = "setup";
    e.message = "scan C:\\Users\\bob\\dir ok";
    std::string js = sb::ui::diagnosticsJson(counters, {e});
    auto parsed = sb::json::Value::parse(js);
    check(parsed.ok() && parsed.value().has("counters") && parsed.value().has("trace"),
          "ui.diagjson-shape");
    if (parsed.ok()) {
      const std::string msg =
          parsed.value().at("trace").asArray()[0].at("msg").asString();
      check(msg.find("bob") == std::string::npos, "ui.diagjson-redacted");
      check(parsed.value().at("counters").at("synthetic.tick").asNumber() == 3.0,
            "ui.diagjson-counter");
    }
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path tmp = fs::temp_directory_path() / "sb-ui-export-test";
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp / "tree", ec);
    { std::ofstream f(tmp / "tree" / "SB-ROOT-MARKER.txt"); f << "marker"; }
    check(!sb::ui::exportTextOutsideRepo((tmp / "tree" / "out.json").string(), "{}").ok(),
          "ui.export-refuse-in-tree");
    check(!sb::ui::exportTextOutsideRepo("", "{}").ok(), "ui.export-reject-empty");
    std::string outside = (tmp / "out.json").string();
    check(sb::ui::exportTextOutsideRepo(outside, "{\"ok\":true}").ok(), "ui.export-ok");
    {
      std::ifstream f(fs::path(outside), std::ios::binary);
      std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
      check(body == "{\"ok\":true}", "ui.export-roundtrip");
    }
    fs::remove_all(tmp, ec);
  }

  // --- ui support: refusal-census summaries for the Diagnostics page ---
  {
    sb::diag::NullCensus z;
    std::string zl = sb::ui::nullCensusLine(z);
    check(zl.find("total=0") != std::string::npos && zl.find("entry=0") != std::string::npos &&
              zl.find("dimension=0") != std::string::npos,
          "ui.refusal-null-zero");
    sb::diag::NullCensus c;
    c.note(sb::diag::NullCause::Entry);
    c.note(sb::diag::NullCause::Entry);
    c.note(sb::diag::NullCause::Token);
    c.note(sb::diag::NullCause::None);  // ignored: not a null binding
    std::string cl = sb::ui::nullCensusLine(c);
    check(cl.find("total=3") != std::string::npos && cl.find("entry=2") != std::string::npos &&
              cl.find("token=1") != std::string::npos && cl.find("buffer=0") != std::string::npos,
          "ui.refusal-null-counts");
    sb::diag::PerReasonCensus p;
    std::string pl = sb::ui::perReasonCensusLine(p);
    check(pl.find("total=0") != std::string::npos && pl.find("overflow=0") != std::string::npos,
          "ui.refusal-reason-zero");
    check(p.note(0x1234, "nope"), "ui.refusal-detail-first");
    check(!p.note(0x1234, "nope"), "ui.refusal-repeat-quiet");
    p.note(0x42, "a\nb");
    check(sb::ui::perReasonCensusLine(p).find("total=3") != std::string::npos,
          "ui.refusal-reason-counts");
    auto det = sb::ui::perReasonDetailLines(p, 8);
    check(det.size() == 3 && det[0].find("total 3") != std::string::npos,
          "ui.refusal-detail-lines");
    check(det[2].find("a b") != std::string::npos, "ui.refusal-detail-sanitized");
    auto capped = sb::ui::perReasonDetailLines(p, 1);
    check(capped.size() == 2, "ui.refusal-detail-cap");
    sb::diag::PerReasonCensus full;
    for (std::uint64_t k = 1; k <= 128; ++k) full.note(k, "r");
    full.note(9999, "overflow-me");
    check(sb::ui::perReasonCensusLine(full).find("overflow=1") != std::string::npos,
          "ui.refusal-overflow-line");
  }

  // --- ui support: read-only shader-cache stats for the Shader Manager ---
  {
    namespace fs = std::filesystem;
    std::error_code ec;
    auto dir = (fs::temp_directory_path() / "sb-ui-cache-test").string();
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    {
      std::ofstream f(fs::path(dir) / "a.bin", std::ios::binary);
      f << "0123456789";
    }
    {
      std::ofstream f(fs::path(dir) / "b.bin", std::ios::binary);
      f << "01234567890123456789";
    }
    {
      std::ofstream f(fs::path(dir) / "c.txt", std::ios::binary);
      f << "ignored";
    }
    const std::size_t big = 512u * 1024u * 1024u;
    sb::ui::CacheStats st = sb::ui::queryCacheStats(dir, 4096, big);
    check(st.entries == 2 && st.bytes == 30, "ui.cache-counts");
    check(!st.overBudget && st.dir == dir, "ui.cache-budget-ok");
    check(sb::ui::cacheStatsLine(st).find("2 entries") != std::string::npos &&
              sb::ui::cacheStatsLine(st).find("within budget") != std::string::npos,
          "ui.cache-line-ok");
    sb::ui::CacheStats tight = sb::ui::queryCacheStats(dir, 4096, 10);
    check(tight.overBudget, "ui.cache-bytes-over");
    check(sb::ui::cacheStatsLine(tight).find("BUDGET EXHAUSTED") != std::string::npos,
          "ui.cache-line-over");
    check(sb::ui::queryCacheStats(dir, 2, big).overBudget, "ui.cache-entries-over");
    auto solo = (fs::temp_directory_path() / "sb-ui-cache-solo").string();
    fs::remove_all(solo, ec);
    fs::create_directories(solo, ec);
    {
      std::ofstream f(fs::path(solo) / "only.bin", std::ios::binary);
      f << "x";
    }
    check(sb::ui::cacheStatsLine(sb::ui::queryCacheStats(solo, 4096, big)).find("1 entry, ") !=
              std::string::npos,
          "ui.cache-line-singular");
    fs::remove_all(solo, ec);
    sb::ui::CacheStats miss = sb::ui::queryCacheStats(
        (fs::temp_directory_path() / "sb-ui-cache-nope").string(), 4096, big);
    check(miss.entries == 0 && miss.bytes == 0 && !miss.overBudget, "ui.cache-missing");
    fs::remove_all(dir, ec);
  }

  // --- frame box: fixed-width frame-rate/load summary + bottleneck rule ---
  {
    using sb::diag::FrameBoxMode;
    check(sb::diag::parseFrameBoxMode("0") == FrameBoxMode::Off, "framebox.mode-off");
    check(sb::diag::parseFrameBoxMode("fps") == FrameBoxMode::Rate, "framebox.mode-rate");
    check(sb::diag::parseFrameBoxMode("") == FrameBoxMode::All, "framebox.mode-default");
    check(sb::diag::parseFrameBoxMode("verbose") == FrameBoxMode::All, "framebox.mode-other");
    check(sb::diag::frameRateText(103) == "10.3 FPS", "framebox.rate-tenth");
    check(sb::diag::frameRateText(7) == "0.7 FPS", "framebox.rate-sub1");
    check(sb::diag::frameRateText(300) == "30.0 FPS", "framebox.rate-30");
    check(sb::diag::frameRateText(99999) == "9999.9 FPS", "framebox.rate-max");
    check(sb::diag::frameRateMsText(58) == "5.8 FPS  172 MS", "framebox.rate-ms");
    check(sb::diag::frameRateMsText(99999) == "9999.9 FPS  0 MS", "framebox.rate-ms-max");
    check(sb::diag::frameRateMsText(0) == "0.0 FPS", "framebox.rate-zero-noms");
    auto t58 = sb::diag::tenthsFromFps(5.8);
    check(t58 && *t58 == 58, "framebox.tenths-round");
    auto t103 = sb::diag::tenthsFromFps(10.3);
    check(t103 && *t103 == 103, "framebox.tenths-103");
    check(!sb::diag::tenthsFromFps(-1.0), "framebox.tenths-negative");
    check(!sb::diag::tenthsFromFps(std::numeric_limits<double>::infinity()),
          "framebox.tenths-inf");
    check(!sb::diag::tenthsFromFps(std::numeric_limits<double>::quiet_NaN()),
          "framebox.tenths-nan");
    auto tbig = sb::diag::tenthsFromFps(1e9);
    check(tbig && *tbig == 99999, "framebox.tenths-clamp");
    check(sb::diag::fitBoxLine("abc", 5) == "abc  ", "framebox.fit-pad");
    check(sb::diag::fitBoxLine("abcdef", 5) == "abcde", "framebox.fit-cut");
    check(sb::diag::fitBoxLine("abcde", 5) == "abcde", "framebox.fit-exact");
    // Full box with the longest figures: every line exactly 26 glyphs.
    sb::diag::FrameBoxStats full;
    full.cpuAll = 100.0;
    full.cores = 28.0;
    full.topThread = 100.0;
    full.gpuKnown = true;
    full.gpuAll = 100.0;
    full.gpuOurs = 100.0;
    full.ramOursMB = 31 * 1024 + 512;
    full.ramFreeMB = 12 * 1024 + 100;
    sb::diag::FrameBoxCard card;
    card.budgetBytes = std::uint64_t(12282) << 20;
    card.usedBytes = std::uint64_t(11900) << 20;
    auto box = sb::diag::frameBoxLines(99999, full, card);
    auto trim = [](std::string s) {
      while (!s.empty() && s.back() == ' ') s.pop_back();
      return s;
    };
    check(box.size() == 6, "framebox.six-lines");
    check(trim(box[0]) == "9999.9 FPS  0 MS", "framebox.line-rate");
    check(trim(box[1]) == "CPU 100%  GAME 28.0 CORES", "framebox.line-cpu");
    check(trim(box[2]) == "TOP THREAD 100%", "framebox.line-thread");
    check(trim(box[3]) == "GPU 100%  GAME 100%", "framebox.line-gpu");
    check(trim(box[4]) == "VRAM 11.6/12.0 GB", "framebox.line-vram");
    check(trim(box[5]) == "RAM 31.5 GB  FREE 12.1 GB", "framebox.line-ram");
    for (const auto& line : box)
      check(line.size() == sb::diag::kFrameBoxColumns, "framebox.width-26");
    // Usual figures with an unknown card, and nothing measured at all.
    sb::diag::FrameBoxStats nogpu = full;
    nogpu.gpuKnown = false;
    auto box2 = sb::diag::frameBoxLines(58, nogpu, card);
    check(trim(box2[0]) == "5.8 FPS  172 MS", "framebox.line-rate-58");
    check(trim(box2[3]) == "GPU NOT KNOWN", "framebox.line-gpu-unknown");
    auto box3 = sb::diag::frameBoxLines(std::nullopt, std::nullopt, std::nullopt);
    check(box3.size() == 6 && trim(box3[0]).empty() && trim(box3[3]).empty(),
          "framebox.all-blank");
    for (const auto& line : box3)
      check(line.size() == sb::diag::kFrameBoxColumns, "framebox.blank-width");
    // Bottleneck rule: saturated card decides first, then a hot thread.
    sb::diag::FrameBoxStats held;
    held.gpuKnown = true;
    held.gpuAll = 41.0;
    held.topThread = 97.0;
    check(sb::diag::classifyBottleneck(held) == sb::diag::Bottleneck::ThreadHeld,
          "framebox.neck-thread");
    held.gpuAll = 95.0;
    check(sb::diag::classifyBottleneck(held) == sb::diag::Bottleneck::GpuHeld,
          "framebox.neck-gpu");
    sb::diag::FrameBoxStats idle;
    idle.gpuKnown = true;
    idle.gpuAll = 10.0;
    idle.topThread = 30.0;
    check(sb::diag::classifyBottleneck(idle) == sb::diag::Bottleneck::Waiting,
          "framebox.neck-waiting");
    sb::diag::FrameBoxStats blind;
    blind.topThread = 30.0;
    check(sb::diag::classifyBottleneck(blind) == sb::diag::Bottleneck::Unknown,
          "framebox.neck-unknown");
    blind.topThread = 97.0;
    check(sb::diag::classifyBottleneck(blind) == sb::diag::Bottleneck::ThreadHeld,
          "framebox.neck-thread-blind");
    check(std::string(sb::diag::bottleneckName(sb::diag::Bottleneck::GpuHeld)) == "gpu-held",
          "framebox.neck-name");
    check(std::string(sb::diag::bottleneckName(sb::diag::Bottleneck::Unknown)) == "unknown",
          "framebox.neck-name-unknown");
  }

  // --- ui support: honest single-sample frame-rate readout for Benchmark ---
  {
    std::string r100 = sb::ui::smokeRateLine(100.0);
    check(r100.find("10.0 FPS  100 MS") != std::string::npos, "ui.smoke-rate-10fps");
    check(r100.find("single smoke sample") != std::string::npos &&
              r100.find("not a sustained stream") != std::string::npos,
          "ui.smoke-rate-honest");
    check(sb::ui::smokeRateLine(1000.0 / 5.8).find("5.8 FPS  172 MS") != std::string::npos,
          "ui.smoke-rate-58");
    check(sb::ui::smokeRateLine(1.0).find("1000.0 FPS  1 MS") != std::string::npos,
          "ui.smoke-rate-fast");
    check(sb::ui::smokeRateLine(1e-9).find("9999.9 FPS") != std::string::npos,
          "ui.smoke-rate-clamp");
    check(sb::ui::smokeRateLine(0.0).find("no frame-rate sample") != std::string::npos,
          "ui.smoke-rate-zero");
    check(sb::ui::smokeRateLine(-3.0).find("no frame-rate sample") != std::string::npos,
          "ui.smoke-rate-negative");
    check(sb::ui::smokeRateLine(std::numeric_limits<double>::quiet_NaN())
                  .find("no frame-rate sample") != std::string::npos,
          "ui.smoke-rate-nan");
    check(sb::ui::smokeRateLine(std::numeric_limits<double>::infinity())
                  .find("no frame-rate sample") != std::string::npos,
          "ui.smoke-rate-inf");
    std::string neck = sb::ui::smokeBottleneckLine();
    check(neck.find("unknown") != std::string::npos &&
              neck.find("no continuous") != std::string::npos,
          "ui.smoke-bottleneck");
  }

  // --- strict CLI parsing: reject unknown/missing/malformed, never default ---
  {
    auto vecs = [](std::initializer_list<const char*> xs) {
      std::vector<std::string> v;
      for (const char* x : xs) v.emplace_back(x);
      return v;
    };
    {
      auto a = vecs({"--top"});
      sb::cli::Parser p(a);
      check(p.nextArg() == std::optional<std::string>("--top"), "cli.next-arg");
      check(p.nextArg() == std::nullopt && p.atEnd(), "cli.end");
      check(p.parseU64("--top").error().message == "--top needs a value", "cli.missing");
    }
    {
      auto a = vecs({"--top", "--full"});
      sb::cli::Parser p(a);
      check(p.nextArg() == std::optional<std::string>("--top"), "cli.next-arg2");
      check(p.parseU64("--top").error().message == "--top needs a value", "cli.flag-as-value");
    }
    {
      auto a = vecs({"--top", "abc"});
      sb::cli::Parser p(a);
      p.nextArg();
      check(p.parseU64("--top").error().message == "invalid value for --top: abc",
            "cli.malformed");
    }
    {
      auto a = vecs({"--top", "-5"});
      sb::cli::Parser p(a);
      p.nextArg();
      check(!p.parseU64("--top").ok(), "cli.negative-u64");
    }
    {
      auto a = vecs({"--top", "128"});
      sb::cli::Parser p(a);
      p.nextArg();
      auto v = p.parseU64("--top");
      check(v.ok() && v.value() == 128, "cli.u64-ok");
    }
    {
      auto a = vecs({"--split", "2"});
      sb::cli::Parser p(a);
      p.nextArg();
      check(p.parseBool01("--split").error().message.find("expected 0 or 1") !=
                std::string::npos,
            "cli.bool01-range");
    }
    {
      auto a = vecs({"--split", "1"});
      sb::cli::Parser p(a);
      p.nextArg();
      auto v = p.parseBool01("--split");
      check(v.ok() && v.value(), "cli.bool01-one");
      auto b = vecs({"--split", "0"});
      sb::cli::Parser q(b);
      q.nextArg();
      auto w = q.parseBool01("--split");
      check(w.ok() && !w.value(), "cli.bool01-zero");
    }
    {
      auto a = vecs({"--hash", "0xZZ"});
      sb::cli::Parser p(a);
      p.nextArg();
      check(p.parseHexU64("--hash").error().message.find("--hash:") != std::string::npos,
            "cli.hex-malformed");
    }
    {
      auto a = vecs({"--hash", "0xDeadBeef"});
      sb::cli::Parser p(a);
      p.nextArg();
      auto v = p.parseHexU64("--hash");
      check(v.ok() && v.value() == 0xDEADBEEFu, "cli.hex-0x");
    }
    {
      auto a = vecs({"--hash", "deadbeef"});
      sb::cli::Parser p(a);
      p.nextArg();
      auto v = p.parseHexU64("--hash");
      check(v.ok() && v.value() == 0xDEADBEEFu, "cli.hex-bare");
    }
    check(!sb::cli::Parser::parseHexU64Str("").ok(), "cli.hex-empty");
    check(!sb::cli::Parser::parseHexU64Str("12345678901234567").ok(), "cli.hex-too-long");
    check(sb::cli::Parser::parseHexU64Str("0X1A").ok(), "cli.hex-cap-0x");
    check(sb::cli::Parser::unexpected("--fnid") == "unknown option: --fnid",
          "cli.unexpected-flag");
    check(sb::cli::Parser::unexpected("-x") == "unknown option: -x", "cli.unexpected-short");
    check(sb::cli::Parser::unexpected("extra") == "unexpected argument: extra",
          "cli.unexpected-positional");
  }

  // --- ui support: Controls scripted-input summaries (DEMO, no device) ---
  {
    using sb::input::parseInputScript;
    check(sb::ui::scriptSummaryLine(sb::input::InputScript()).find("empty") !=
                std::string::npos,
          "ui.script-empty");
    {
      auto s = parseInputScript("1@0+0.3, 2@0.5, lt=1@1.0");
      const std::string summary = sb::ui::scriptSummaryLine(s);
      check(summary.find("3 entries") != std::string::npos &&
                summary.find("0 rejections") != std::string::npos &&
                summary.find("base=poll") != std::string::npos &&
                summary.find("span 0.0..1.3 s") != std::string::npos,
            "ui.script-summary");
      check(sb::ui::scriptDetailLines(s, 8).empty(), "ui.script-clean-no-detail");
      const std::string held =
          sb::ui::scriptStateLine(sb::input::playInputScript(s, 0.1));
      check(held.find("buttons=0x1") != std::string::npos &&
                held.find("lt=0.00") != std::string::npos,
            "ui.script-state-button");
      const std::string trig =
          sb::ui::scriptStateLine(sb::input::playInputScript(s, 1.1));
      check(trig.find("buttons=0x0") != std::string::npos &&
                trig.find("lt=1.00") != std::string::npos,
            "ui.script-state-trigger");
    }
    {
      auto s = parseInputScript("base=start, 4@1, wait, zz@2");
      const std::string summary = sb::ui::scriptSummaryLine(s);
      check(summary.find("1 entry") != std::string::npos &&
                summary.find("2 rejections") != std::string::npos &&
                summary.find("base=start") != std::string::npos,
            "ui.script-summary-reject");
      auto lines = sb::ui::scriptDetailLines(s, 8);
      check(lines.size() == 2 && lines[0].find("unsupported directive") != std::string::npos &&
                lines[1].find("bad mask") != std::string::npos,
            "ui.script-detail-reasons");
      check(sb::ui::scriptDetailLines(s, 1).size() == 1, "ui.script-detail-cap");
    }
    {
      std::string longText(200, 'z');
      auto s = parseInputScript(std::string("4@1,") + longText + "@2");
      auto lines = sb::ui::scriptDetailLines(s, 8);
      check(lines.size() == 1 && lines[0].size() < 120 &&
                lines[0].find("...") != std::string::npos,
            "ui.script-detail-truncates");
    }
  }

  // --- engine stats: instance names + busiest engine kind (OS collection refused) ---
  {
    using sb::diag::EngineSample;
    auto parse = sb::diag::parseEngineInstance;
    {
      auto v = parse("pid_1234_luid_0x00000000_0x0000E3B5_phys_0_eng_0_engtype_3D");
      check(v.has_value() && v->first == 1234 && v->second == "3D", "engines.parse-3d");
    }
    {
      auto v = parse("pid_8_luid_0x0_0x1_phys_0_eng_5_engtype_Compute_0");
      check(v.has_value() && v->first == 8 && v->second == "Compute_0",
            "engines.parse-compute");
    }
    check(!parse("_Total").has_value(), "engines.parse-total-none");
    check(!parse("pid_x_engtype_3D").has_value(), "engines.parse-bad-pid-none");
    check(!parse("pid_1_no_kind").has_value(), "engines.parse-no-kind-none");
    check(!parse("pid_4294967296_engtype_3D").has_value(), "engines.parse-overflow-none");
    {
      auto e = [](std::uint32_t pid, const char* kind, double pct) {
        EngineSample s;
        s.pid = pid;
        s.kind = kind;
        s.percent = pct;
        return s;
      };
      const std::vector<EngineSample> engines = {e(1, "3D", 30.0), e(2, "3D", 25.0),
                                                e(1, "Copy", 40.0), e(2, "VideoEncode", 70.0),
                                                e(1, "3D", 5.0)};
      const auto all = sb::diag::busiestEngineKind(engines, std::nullopt);
      check(all.top == 70.0f && all.byKind.at("3D") == 60.0 &&
                all.byKind.at("Copy") == 40.0 && all.byKind.at("VideoEncode") == 70.0,
            "engines.busiest-all");
      check(sb::diag::busiestEngineKind(engines, 1u).top == 40.0f, "engines.busiest-pid1");
      const auto missing = sb::diag::busiestEngineKind(engines, 9u);
      check(missing.top == 0.0f && missing.byKind.empty(), "engines.busiest-missing");
      const std::vector<EngineSample> over = {e(1, "3D", 80.0), e(1, "3D", 60.0)};
      check(sb::diag::busiestEngineKind(over, std::nullopt).top == 100.0f,
            "engines.busiest-clamp");
    }
  }

  // --- ui support: Diagnostics engine-instance probe (no live figures) ---
  {
    check(sb::ui::engineInstanceLine(
                "pid_1234_luid_0x00000000_0x0000E3B5_phys_0_eng_0_engtype_3D") ==
                "pid=1234 kind=3D",
          "ui.engine-probe-3d");
    check(sb::ui::engineInstanceLine("pid_8_luid_0x0_0x1_phys_0_eng_5_engtype_Compute_0") ==
                "pid=8 kind=Compute_0",
          "ui.engine-probe-compute");
    check(sb::ui::engineInstanceLine("_Total").find("not an engine instance name") !=
                std::string::npos,
          "ui.engine-probe-notaname");
    check(sb::ui::engineInstanceLine("pid_7_engtype_") == "pid=7 kind=<empty>",
          "ui.engine-probe-empty-kind");
  }

  // --- shader batch fail-tally: reason -> (count, first example) ---
  {
    {
      sb::gfx::FailTally t;
      check(!t.list() && t.total() == 0 && t.reasons() == 0 && t.report().empty() &&
                t.listLines().empty(),
            "tally.empty");
      auto unseen = t.countFor("nope");
      check(unseen.first == 0 && unseen.second.empty(), "tally.unseen");
    }
    {
      sb::gfx::FailTally t;
      t.fail("emit: no lane", "a.hlsl");
      t.fail("emit: no lane", "b.hlsl");
      t.fail("emit: no lane", "c.hlsl");
      auto c = t.countFor("emit: no lane");
      check(c.first == 3 && c.second == "a.hlsl", "tally.first-example-wins");
      check(t.total() == 3 && t.reasons() == 1, "tally.totals");
      check(t.listLines().empty(), "tally.nonlist-no-lines");
    }
    {
      sb::gfx::FailTally t(true);
      check(t.list(), "tally.list-flag");
      t.fail("emit: no lane", "b.hlsl");
      t.fail("emit: no lane", "a.hlsl");
      // List lines sort ascending, independent of failure order.
      check(t.listLines() == std::vector<std::string>{"a.hlsl  emit: no lane",
                                                      "b.hlsl  emit: no lane"},
            "tally.list-lines-sorted");
    }
    {
      sb::gfx::FailTally a, b(true);
      a.fail("r1", "ours");
      a.fail("r1", "ours-again");
      b.fail("r1", "theirs");
      b.fail("r2", "other");
      a.merge(b);
      auto r1 = a.countFor("r1");
      check(r1.first == 3 && r1.second == "ours", "tally.merge-keeps-first");
      check(a.countFor("r2").first == 1 && a.total() == 4 && a.reasons() == 2,
            "tally.merge-sums");
    }
    {
      sb::gfx::FailTally a(true), b(true);
      a.fail("r1", "z.hlsl");
      b.fail("r2", "a.hlsl");
      a.merge(b);
      check(a.listLines() == std::vector<std::string>{"a.hlsl  r2", "z.hlsl  r1"},
            "tally.merge-lines");
      sb::gfx::FailTally e;
      a.merge(e);
      check(a.total() == 2 && a.reasons() == 2, "tally.merge-empty");
    }
    {
      sb::gfx::FailTally t;
      t.fail("r-low", "low.hlsl");
      for (int i = 0; i < 5; ++i) t.fail("r-high", "high.hlsl");
      auto rep = t.report();
      check(rep.size() == 2 && rep[0] == "        5  r-high  (e.g. high.hlsl)" &&
                rep[1] == "        1  r-low  (e.g. low.hlsl)",
            "tally.report-order-format");
    }
    {
      sb::gfx::FailTally t;
      t.fail("r-b", "b.hlsl");
      t.fail("r-a", "a.hlsl");
      auto rep = t.report();
      // Tied counts keep reason-ascending order (stable sort over the
      // reason-ordered map, like the upstream sort over its BTreeMap).
      check(rep.size() == 2 && rep[0].find("r-a") != std::string::npos &&
                rep[1].find("r-b") != std::string::npos,
            "tally.report-ties");
    }
  }

  // --- ui support: Setup scan report (redacted text, for sharing) ---
  {
    sb::ui::FolderScan ok;
    ok.ok = true;
    ok.shownDir = "<redacted>";
    ok.entries = 12;
    ok.bytes = 345;
    ok.hashed = 3;
    ok.ms = 2.5;
    const std::string t = sb::ui::folderReportText(ok);
    check(t.find("dir: <redacted>") != std::string::npos, "ui.scan-report-redacted");
    check(t.find("entries: 12") != std::string::npos &&
                t.find("bytes: 345") != std::string::npos &&
                t.find("hashed: 3") != std::string::npos,
          "ui.scan-report-counts");
    check(t.find("scan_ms: 2.5") != std::string::npos, "ui.scan-report-ms");
    check(t.find("status: ok") != std::string::npos, "ui.scan-report-ok");
    sb::ui::FolderScan bad;
    bad.ok = false;
    bad.shownDir = "C:/x";
    bad.error = "not a directory";
    const std::string f = sb::ui::folderReportText(bad);
    check(f.find("status: failed: not a directory") != std::string::npos,
          "ui.scan-report-failed");
    check(f.find("dir: C:/x") != std::string::npos, "ui.scan-report-plain-dir");
  }

  // --- declared-output compare: Unknown/Unreadable/Same/Changed + partial ---
  {
    using sb::diag::CompareSnapshot;
    auto snap = [](const char* s) {
      CompareSnapshot b{};
      std::size_t n = 0;
      while (s[n] != '\0' && n < sb::diag::kMaxSnapshotBytes) {
        b[n] = static_cast<unsigned char>(s[n]);
        ++n;
      }
      return b;
    };
    auto bytes = [](std::size_t a, std::size_t b) {
      return std::array<std::size_t, sb::diag::kMaxCompareOutputs>{a, b};
    };
    auto compared = [](bool a, bool b) {
      return std::array<bool, sb::diag::kMaxCompareOutputs>{a, b};
    };
    {
      auto r = sb::diag::decideCompare(nullptr, compared(true, true),
                                       {snap("ab"), snap("cd")},
                                       {snap("ab"), snap("cd")}, bytes(2, 2));
      check(r.outcome == sb::diag::CompareOutcome::Unknown, "compare-null-outcome");
      check(sb::diag::formatCompare(r, nullptr) == "?", "compare-null-text");
    }
    {
      sb::diag::OutputContract c;
      c.first = 1;
      auto r = sb::diag::decideCompare(&c, compared(false, false),
                                       {snap("ab"), snap("")},
                                       {snap("ab"), snap("")}, bytes(2, 0));
      check(r.outcome == sb::diag::CompareOutcome::Unreadable, "compare-unreadable");
      check(sb::diag::formatCompare(r, &c) == "unreadable", "compare-unreadable-text");
    }
    {
      sb::diag::OutputContract c;
      c.first = 1;
      auto r = sb::diag::decideCompare(&c, compared(true, false),
                                       {snap("ab"), snap("")},
                                       {snap("ab"), snap("")}, bytes(2, 0));
      check(r.outcome == sb::diag::CompareOutcome::Same && !r.partial, "compare-same");
      check(sb::diag::formatCompare(r, &c) == "same", "compare-same-text");
    }
    {
      sb::diag::OutputContract c;
      c.first = 1;
      auto r = sb::diag::decideCompare(&c, compared(true, false),
                                       {snap("ab"), snap("")},
                                       {snap("aB"), snap("")}, bytes(2, 0));
      check(r.outcome == sb::diag::CompareOutcome::Changed && r.changedCount == 1 &&
                r.changed[0] == 0,
            "compare-changed-first");
      check(sb::diag::formatCompare(r, &c) == "changed:1", "compare-changed-first-text");
    }
    {
      sb::diag::OutputContract c;
      c.first = 3;
      c.second = 4;
      auto r = sb::diag::decideCompare(&c, compared(true, true),
                                       {snap("ab"), snap("cd")},
                                       {snap("ab"), snap("cD")}, bytes(2, 2));
      check(r.outcome == sb::diag::CompareOutcome::Changed, "compare-changed-second");
      check(sb::diag::formatCompare(r, &c) == "changed:4", "compare-changed-second-text");
    }
    {
      sb::diag::OutputContract c;
      c.first = 3;
      c.second = 4;
      auto r = sb::diag::decideCompare(&c, compared(true, false),
                                       {snap("ab"), snap("")},
                                       {snap("ab"), snap("")}, bytes(2, 0));
      check(r.outcome == sb::diag::CompareOutcome::Same && r.partial, "compare-same-partial");
      check(sb::diag::formatCompare(r, &c) == "same,partial", "compare-same-partial-text");
    }
    {
      sb::diag::OutputContract c;
      c.first = 3;
      c.second = 4;
      auto r = sb::diag::decideCompare(&c, compared(false, true),
                                       {snap(""), snap("cd")},
                                       {snap(""), snap("cD")}, bytes(0, 2));
      check(r.outcome == sb::diag::CompareOutcome::Changed && r.partial,
            "compare-changed-partial");
      check(sb::diag::formatCompare(r, &c) == "changed:4,partial",
            "compare-changed-partial-text");
    }
    {
      sb::diag::OutputContract c;
      c.first = 1;
      auto r = sb::diag::decideCompare(&c, compared(true, false),
                                       {snap("ab"), snap("")},
                                       {snap("aB"), snap("")}, bytes(0, 0));
      check(r.outcome == sb::diag::CompareOutcome::Unreadable, "compare-zero-bytes");
    }
    {
      // Widths past the snapshot bound compare within it (64 equal bytes).
      sb::diag::OutputContract c;
      c.first = 0;
      CompareSnapshot full;
      full.fill(0x41);
      auto r = sb::diag::decideCompare(&c, compared(true, false), {full, snap("")},
                                       {full, snap("")}, bytes(100, 0));
      check(r.outcome == sb::diag::CompareOutcome::Same, "compare-clamp-bound");
    }
    {
      sb::diag::OutputContract c;
      c.first = 3;
      c.second = 4;
      auto r = sb::diag::decideCompare(&c, compared(true, true),
                                       {snap("ab"), snap("cd")},
                                       {snap("aB"), snap("cD")}, bytes(2, 2));
      check(sb::diag::formatCompare(r, &c) == "changed:3,4", "compare-both-text");
    }
  }

  // --- ui support: Shader Manager failure-tally view (real FailTally, zeros honest) ---
  {
    sb::gfx::FailTally empty;
    check(sb::ui::failTallyLine(empty) == "shader failures total=0 reasons=0",
          "ui.tally-zero");
    auto emptyLines = sb::ui::failTallyReportLines(empty, 8);
    check(emptyLines.size() == 1 && emptyLines[0].find("total=0") != std::string::npos,
          "ui.tally-report-zero");
    sb::gfx::FailTally t;
    t.fail("emit: no lane", "a.hlsl");
    t.fail("emit: no lane", "b.hlsl");
    t.fail("stage: bad token", "c.hlsl");
    check(sb::ui::failTallyLine(t) == "shader failures total=3 reasons=2",
          "ui.tally-counts");
    auto lines = sb::ui::failTallyReportLines(t, 8);
    check(lines.size() == 3 && lines[0].find("total=3") != std::string::npos,
          "ui.tally-report-summary");
    check(lines[1].find("emit: no lane") != std::string::npos &&
              lines[1].find("a.hlsl") != std::string::npos,
          "ui.tally-report-first");
    check(sb::ui::failTallyReportLines(t, 1).size() == 2, "ui.tally-report-cap");
    sb::gfx::FailTally merged;
    merged.fail("r1", "ours");
    sb::gfx::FailTally other(true);
    other.fail("r1", "theirs");
    other.fail("r2", "other");
    merged.merge(other);
    check(sb::ui::failTallyLine(merged).find("total=3") != std::string::npos,
          "ui.tally-merge-counts");
  }

  // --- driver-error trip latch: first error trips, every error counted ---
  {
    sb::diag::DriverWatch w;
    check(!w.tripped() && w.errors() == 0, "drvwatch.fresh");
    check(w.noteError(), "drvwatch.first-trips");
    check(w.tripped() && w.errors() == 1, "drvwatch.tripped-once");
    check(!w.noteError(), "drvwatch.second-no-trip");
    check(w.tripped() && w.errors() == 2, "drvwatch.counts-all");
    for (int i = 0; i < 3; ++i) w.noteError();
    check(w.errors() == 5, "drvwatch.keeps-counting");
    w.reset();
    check(!w.tripped() && w.errors() == 0, "drvwatch.reset");
    check(w.noteError() && w.errors() == 1, "drvwatch.retrips");
    sb::diag::DriverWatch other;
    check(!other.tripped() && other.errors() == 0, "drvwatch.independent");
  }

  std::printf("pass=%d fail=%d\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
