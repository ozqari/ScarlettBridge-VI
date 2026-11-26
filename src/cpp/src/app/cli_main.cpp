// scarlettbridge-cli: headless automation for tests and diagnostics.
// Subcommands: version | probe | validate-folder <dir> [--redact] |
//   run-fixture <sbm1> [--census] | pe-census <pe> [--allow-system] |
//   pe-exports <pe> | translate <sbvs> | cache ...
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "app/cli_parse.h"
#include "core/config.h"
#include "core/hash.h"
#include "core/logging.h"
#include "core/version.h"
#include "formats/pe_exports.h"
#include "formats/pe_imports.h"
#include "graphics/abstraction.h"
#include "graphics/pipeline_cache.h"
#include "graphics/shader_translate.h"
#include "platform/windows_system.h"
#include "runtime/imports.h"
#include "runtime/sbm.h"

namespace {
int usage() {
  std::cout << "usage: scarlettbridge-cli <version|probe|validate-folder|run-fixture|pe-census|pe-exports|translate|cache> ...\n";
  return 2;
}

std::string jsonEscape(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}

// Strict argument failure: machine-readable error plus the usage line.
int argError(const std::string& msg) {
  std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(msg) << "\"}\n";
  return usage();
}

// Read-only folder scan: names/sizes + SHA-256 of first 64 KiB for files
// under 5 MiB (max 40 hashed), max 500 entries. Never writes into the folder.
int validateFolder(const std::string& dir, bool redact) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) {
    std::cout << "{\"ok\":false,\"error\":\"not a directory\"}\n";
    return 1;
  }
  std::size_t entries = 0, hashed = 0;
  std::uint64_t total = 0;
  auto t0 = std::chrono::steady_clock::now();
  for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator();
       ++it) {
    if (ec) break;
    if (entries >= 500) break;
    if (!it->is_regular_file(ec)) continue;
    ++entries;
    std::uint64_t sz = it->file_size(ec);
    total += sz;
    if (sz < 5u * 1024u * 1024u && hashed < 40) {
      std::ifstream f(it->path(), std::ios::binary);
      char buf[65536];
      f.read(buf, sizeof(buf));
      (void)sb::sha256hex(buf, static_cast<std::size_t>(f.gcount()));
      ++hashed;
    }
  }
  auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  std::string shown = redact ? "<redacted>" : dir;
  std::cout << "{\"ok\":true,\"dir\":\"" << shown << "\",\"entries\":" << entries
            << ",\"bytes\":" << total << ",\"hashed\":" << hashed << ",\"ms\":" << ms << "}\n";
  return 0;
}

int runFixture(const std::string& path, bool census) {
  std::ifstream f(std::filesystem::path(path), std::ios::binary);
  if (!f) {
    std::cout << "{\"ok\":false,\"error\":\"cannot open fixture\"}\n";
    return 1;
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());
  auto mod = sb::sbm::parse(bytes.data(), bytes.size());
  if (!mod.ok()) {
    std::cout << "{\"ok\":false,\"error\":\"" << mod.error().message << "\"}\n";
    return 1;
  }
  sb::ImportRegistry reg;
  reg.registerService("sb.memory.v1", 1);
  reg.registerService("sb.clock.v1", 1);
  reg.registerService("sb.sync.v1", 1);
  reg.registerService("sb.file.readonly.v1", 1);
  if (census) {
    // Classify without running anything (cf. `victory census`).
    // Each row carries the honesty fields from Xbox2Windows import routing:
    // route_kind="candidate", resolution="not_checked" — a module-name
    // classification only, never a proven provider.
    std::cout << "{\"ok\":true,\"census\":[";
    bool first = true;
    for (const auto& im : mod.value().imports) {
      if (!first) std::cout << ",";
      first = false;
      auto c = reg.census(path, im.name);
      std::cout << "{\"symbol\":\"" << im.name << "\",\"class\":\"" << sb::toString(c.klass)
                << "\",\"route_kind\":\"candidate\",\"resolution\":\"not_checked\",\"provider\":\""
                << (c.provider.empty() ? "" : c.provider) << "\"}";
    }
    std::cout << "]}\n";
    return 0;
  }
  bool ok = true;
  for (const auto& im : mod.value().imports) {
    if (auto r = reg.resolve(path, im.name, im.minVersion); !r.ok()) {
      std::cout << "{\"ok\":false,\"unresolved\":\"" << im.name << "\"}\n";
      ok = false;
    }
  }
  if (ok) std::cout << "{\"ok\":true,\"imports\":" << mod.value().imports.size() << "}\n";
  return ok ? 0 : 1;
}

// PE import census: reads one AMD64 image, classifies every normal + delay
// import without loading or executing it. Classification is the registry's
// own censusDll verdict (single source with the loader), labelled with the
// honesty fields route_kind="candidate", resolution="not_checked".
// --allow-system widens the host policy to the well-known system DLL/API-set
// shape for reporting parity; without it every system DLL is host-denied.
int peCensus(const std::string& path, bool allowSystem) {
  auto esc = [](const std::string& s) {
    std::string o;
    for (char c : s) {
      if (c == '"' || c == '\\') o += '\\';
      o += c;
    }
    return o;
  };
  std::ifstream f(std::filesystem::path(path), std::ios::binary);
  if (!f) {
    std::cout << "{\"ok\":false,\"error\":\"cannot open file\"}\n";
    return 1;
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());
  if (bytes.size() < 2 || !(bytes[0] == 'M' && bytes[1] == 'Z')) {
    std::cout << "{\"ok\":false,\"error\":\"not a PE image (missing MZ)\"}\n";
    return 1;
  }
  if (bytes.size() > 256u * 1024u * 1024u) {
    std::cout << "{\"ok\":false,\"error\":\"file over 256 MiB cap\"}\n";
    return 1;
  }
  auto pe = sb::pe::parseImports(bytes.data(), bytes.size());
  if (!pe.ok()) {
    std::cout << "{\"ok\":false,\"error\":\"" << esc(pe.error().message) << "\"}\n";
    return 1;
  }
  sb::ImportRegistry reg;
  if (allowSystem) {
    sb::HostPolicy pol;
    pol.allowHostModule("api-ms-win-*");
    pol.allowHostModule("ext-ms-*");
    for (const char* d : {"kernel32.dll", "kernelbase.dll", "ntdll.dll", "ucrtbase.dll",
                           "msvcrt.dll", "msvcp_win.dll", "vcruntime140.dll",
                           "vcruntime140_1.dll", "msvcp140.dll", "ole32.dll", "oleaut32.dll",
                           "advapi32.dll", "user32.dll", "gdi32.dll", "shell32.dll",
                           "ws2_32.dll", "winhttp.dll", "crypt32.dll", "bcrypt.dll",
                           "gameinput.dll", "xaudio2_9.dll", "mfplat.dll", "mfreadwrite.dll",
                           "dxgi.dll", "d3d12.dll", "d3dcompiler_47.dll", "dbghelp.dll",
                           "version.dll", "winmm.dll", "powrprof.dll", "setupapi.dll",
                           "cfgmgr32.dll", "combase.dll", "rpcrt4.dll", "shlwapi.dll",
                           "userenv.dll", "psapi.dll"})
      pol.allowHostModule(d);
    reg.setHostPolicy(pol);
  }
  struct Row {
    sb::pe::PeImportRow row;
    sb::CensusEntry census;
  };
  std::vector<Row> rows;
  for (const auto& r : pe.value().imports) rows.push_back(Row{r, reg.censusDll(r.module, r.symbol)});
  auto lower = [](const std::string& s) {
    std::string o = s;
    for (char& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return o;
  };
  std::sort(rows.begin(), rows.end(), [&](const Row& a, const Row& b) {
    int ka = a.row.kind == "normal" ? 0 : 1, kb = b.row.kind == "normal" ? 0 : 1;
    if (ka != kb) return ka < kb;
    std::string ma = lower(a.row.module), mb = lower(b.row.module);
    if (ma != mb) return ma < mb;
    bool ao = !a.row.symbol.empty() && a.row.symbol[0] == '#';
    bool bo = !b.row.symbol.empty() && b.row.symbol[0] == '#';
    if (ao != bo) return ao < bo;
    if (ao && bo) {
      try {
        return std::stoul(a.row.symbol.substr(1)) < std::stoul(b.row.symbol.substr(1));
      } catch (...) {
      }
    }
    std::string sa = lower(a.row.symbol), sb2 = lower(b.row.symbol);
    if (sa != sb2) return sa < sb2;
    return sb::toString(a.census.klass) < sb::toString(b.census.klass);
  });
  std::size_t normal = 0, delay = 0, ours = 0, ha = 0, hd = 0, miss = 0, trap = 0;
  for (const auto& r : rows) {
    if (r.row.kind == "normal")
      ++normal;
    else
      ++delay;
    switch (r.census.klass) {
      case sb::ImportClass::Ours: ++ours; break;
      case sb::ImportClass::HostAllowed: ++ha; break;
      case sb::ImportClass::HostDenied: ++hd; break;
      case sb::ImportClass::Missing: ++miss; break;
      case sb::ImportClass::StubTrap: ++trap; break;
    }
  }
  std::cout << "{\"ok\":true,\"image\":\"" << esc(path)
            << "\",\"machine\":\"AMD64\",\"imports\":" << rows.size()
            << ",\"normal\":" << normal << ",\"delay\":" << delay << ",\"rows\":[";
  bool first = true;
  for (const auto& r : rows) {
    if (!first) std::cout << ",";
    first = false;
    std::cout << "{\"kind\":\"" << r.row.kind << "\",\"module\":\"" << esc(r.row.module)
              << "\",\"symbol\":\"" << esc(r.row.symbol) << "\",\"class\":\""
              << sb::toString(r.census.klass) << "\",\"route_kind\":\"candidate\","
              << "\"resolution\":\"not_checked\",\"provider\":\"" << esc(r.census.provider) << "\"}";
  }
  std::cout << "],\"summary\":{\"ours\":" << ours << ",\"hostAllowed\":" << ha
            << ",\"hostDenied\":" << hd << ",\"missing\":" << miss << ",\"stubTrap\":" << trap
            << "}}\n";
  return 0;
}

// PE export census: reads one AMD64 image, lists its export directory
// (names, ordinals, RVAs, forwarder strings) plus image features
// (TLS / exception / load-config presence). Never loads or executes.
int peExports(const std::string& path) {
  auto esc = [](const std::string& s) {
    std::string o;
    for (char c : s) {
      if (c == '"' || c == '\\') o += '\\';
      o += c;
    }
    return o;
  };
  std::ifstream f(std::filesystem::path(path), std::ios::binary);
  if (!f) {
    std::cout << "{\"ok\":false,\"error\":\"cannot open file\"}\n";
    return 1;
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());
  if (bytes.size() < 2 || !(bytes[0] == 'M' && bytes[1] == 'Z')) {
    std::cout << "{\"ok\":false,\"error\":\"not a PE image (missing MZ)\"}\n";
    return 1;
  }
  if (bytes.size() > 256u * 1024u * 1024u) {
    std::cout << "{\"ok\":false,\"error\":\"file over 256 MiB cap\"}\n";
    return 1;
  }
  auto ex = sb::pe::parseExports(bytes.data(), bytes.size());
  if (!ex.ok()) {
    std::cout << "{\"ok\":false,\"error\":\"" << esc(ex.error().message) << "\"}\n";
    return 1;
  }
  auto rows = ex.value().exports;
  std::sort(rows.begin(), rows.end(), sb::pe::exportLess);
  const auto& feat = ex.value().features;
  std::size_t named = 0, fwd = 0;
  for (const auto& r : rows) {
    if (r.name.has_value()) ++named;
    if (r.forwarder.has_value()) ++fwd;
  }
  std::cout << "{\"ok\":true,\"image\":\"" << esc(path) << "\",\"machine\":\"AMD64\",\"exports\":"
            << rows.size() << ",\"named\":" << named << ",\"forwarded\":" << fwd
            << ",\"features\":{\"tls\":" << (feat.hasTls ? "true" : "false")
            << ",\"exception\":" << (feat.hasException ? "true" : "false")
            << ",\"loadConfig\":" << (feat.hasLoadConfig ? "true" : "false") << "},\"rows\":[";
  bool first = true;
  for (const auto& r : rows) {
    if (!first) std::cout << ",";
    first = false;
    std::cout << "{\"ordinal\":" << r.ordinal << ",\"name\":";
    if (r.name.has_value())
      std::cout << "\"" << esc(*r.name) << "\"";
    else
      std::cout << "null";
    std::cout << ",\"rva\":";
    if (r.rva.has_value())
      std::cout << r.rva.value();
    else
      std::cout << "null";
    std::cout << ",\"forwarder\":";
    if (r.forwarder.has_value())
      std::cout << "\"" << esc(*r.forwarder) << "\"";
    else
      std::cout << "null";
    std::cout << "}";
  }
  std::cout << "]}\n";
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  sb::log::init("", sb::log::Severity::Warn, sb::log::Severity::Error);
  if (argc < 2) return usage();
  std::string cmd = argv[1];
  std::vector<std::string> rest(argv + 2, argv + argc);
  if (cmd == "version") {
    if (!rest.empty()) return argError(sb::cli::Parser::unexpected(rest[0]));
    std::cout << "scarlettbridge-cli " << sb::kAppVersion << " ipc=" << sb::kIpcProtocolVersion
              << " schema=" << sb::kConfigSchemaVersion << "\n";
    return 0;
  }
  if (cmd == "probe") {
    if (!rest.empty()) return argError(sb::cli::Parser::unexpected(rest[0]));
    auto sys = sb::sysinfo::collect();
    auto backend = sb::gfx::d3d12Backend();
    std::cout << "{\"os\":\"" << (sys.ok() ? sys.value().osVersion : "?") << "\"";
    if (backend.ok()) {
      auto adapters = backend.value()->enumerateAdapters(false);
      if (adapters.ok()) {
        std::cout << ",\"adapters\":[";
        bool first = true;
        for (const auto& a : adapters.value()) {
          if (!first) std::cout << ",";
          first = false;
          std::cout << "{\"desc\":\"" << a.description << "\",\"mb\":" << a.dedicatedMB
                    << ",\"sw\":" << (a.isSoftware ? "true" : "false") << "}";
        }
        std::cout << "]";
        if (backend.value()->createDevice(0, false, false).ok()) {
          auto smoke = backend.value()->smokeTriangle();
          if (smoke.ok()) {
            const auto& s = smoke.value();
            std::cout << ",\"smoke\":{\"compileMs\":" << s.compileMs << ",\"drawMs\":" << s.drawMs
                      << ",\"pixel\":\"" << s.pixelHash << "\",\"expected\":"
                      << (s.pixelAsExpected ? "true" : "false") << "}";
          } else {
            std::cout << ",\"smokeError\":\"" << smoke.error().message << "\"";
          }
          backend.value()->destroyDevice();
        }
      } else {
        std::cout << ",\"adapterError\":\"" << adapters.error().message << "\"";
      }
    } else {
      std::cout << ",\"backend\":\"" << backend.error().message << "\"";
    }
    std::cout << "}\n";
    return 0;
  }
  if (cmd == "validate-folder") {
    sb::cli::Parser p(rest);
    auto dir = p.nextArg();
    if (!dir) return argError("validate-folder needs a directory");
    if (!dir->empty() && (*dir)[0] == '-')
      return argError(sb::cli::Parser::unexpected(*dir));
    bool redact = false;
    while (auto a = p.nextArg()) {
      if (*a == "--redact")
        redact = true;
      else
        return argError(sb::cli::Parser::unexpected(*a));
    }
    return validateFolder(*dir, redact);
  }
  if (cmd == "run-fixture") {
    sb::cli::Parser p(rest);
    auto path = p.nextArg();
    if (!path) return argError("run-fixture needs a fixture file");
    if (!path->empty() && (*path)[0] == '-')
      return argError(sb::cli::Parser::unexpected(*path));
    bool census = false;
    while (auto a = p.nextArg()) {
      if (*a == "--census")
        census = true;
      else
        return argError(sb::cli::Parser::unexpected(*a));
    }
    return runFixture(*path, census);
  }
  if (cmd == "pe-census") {
    sb::cli::Parser p(rest);
    auto path = p.nextArg();
    if (!path) return argError("pe-census needs a file");
    if (!path->empty() && (*path)[0] == '-')
      return argError(sb::cli::Parser::unexpected(*path));
    bool allow = false;
    while (auto a = p.nextArg()) {
      if (*a == "--allow-system")
        allow = true;
      else
        return argError(sb::cli::Parser::unexpected(*a));
    }
    return peCensus(*path, allow);
  }
  if (cmd == "pe-exports") {
    sb::cli::Parser p(rest);
    auto path = p.nextArg();
    if (!path) return argError("pe-exports needs a file");
    if (!path->empty() && (*path)[0] == '-')
      return argError(sb::cli::Parser::unexpected(*path));
    if (auto a = p.nextArg()) return argError(sb::cli::Parser::unexpected(*a));
    return peExports(*path);
  }
  if (cmd == "translate") {
    sb::cli::Parser p(rest);
    auto path = p.nextArg();
    if (!path) return argError("translate needs a file");
    if (!path->empty() && (*path)[0] == '-') return argError(sb::cli::Parser::unexpected(*path));
    if (auto a = p.nextArg()) return argError(sb::cli::Parser::unexpected(*a));
    std::ifstream f(std::filesystem::path(*path));
    if (!f) {
      std::cout << "{\"ok\":false,\"error\":\"cannot open sbvs\"}\n";
      return 1;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    auto t = sb::shader::translateText(ss.str(), false);
    if (!t.ok()) {
      std::cout << "{\"ok\":false,\"error\":\"" << t.error().message << "\"}\n";
      return 1;
    }
    if (t.value().dxbc.empty()) {
      // Capability skip (77): parsed + emitted, but no compile validation on
      // this machine. Reported separately from pass/fail.
      std::cout << "{\"skip\":true,\"hlslBytes\":" << t.value().hlsl.size() << "}\n";
      return 77;
    }
    std::cout << "{\"ok\":true,\"hlslBytes\":" << t.value().hlsl.size()
              << ",\"dxbcBytes\":" << t.value().dxbc.size() << "}\n";
    return 0;
  }
  return usage();
}
