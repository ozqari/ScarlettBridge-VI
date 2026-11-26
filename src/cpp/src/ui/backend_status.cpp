#include "ui/backend_status.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include "core/filesystem_guards.h"
#include "core/hash.h"
#include "core/json.h"
#include "core/logging.h"
#include "core/version.h"
#include "graphics/abstraction.h"
#include "graphics/pipeline_cache.h"
#include "graphics/shader_tally.h"
#include "media/audio_backend.h"
#include "media/input_backend.h"
#include "media/input_script.h"
#include "platform/windows_system.h"
#include "telemetry/engine_stats.h"
#include "telemetry/frame_box.h"

namespace sb {
namespace ui {

Versions versions() {
  Versions v;
  v.app = kAppVersion;
  v.build = kBuildId;
  v.ipc = std::to_string(kIpcProtocolVersion);
  v.schema = std::to_string(kConfigSchemaVersion);
  v.translator = std::to_string(kTranslatorVersion);
  v.cache = std::to_string(kPipelineCacheVersion);
  v.sbm = std::to_string(kSbmFormatVersion);
  return v;
}

FolderScan scanFolderReadOnly(const std::string& dir, bool redact) {
  FolderScan out;
  out.shownDir = redact ? "<redacted>" : dir;
  auto t0 = std::chrono::steady_clock::now();
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) {
    out.ok = false;
    out.error = "not a directory";
    return out;
  }
  std::size_t entries = 0, hashed = 0;
  std::uint64_t total = 0;
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
      if (f) {
        char buf[65536];
        f.read(buf, sizeof(buf));
        (void)sha256hex(buf, static_cast<std::size_t>(f.gcount()));
        ++hashed;
      }
    }
  }
  auto ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  out.ok = true;
  out.entries = entries;
  out.bytes = total;
  out.hashed = hashed;
  out.ms = ms;
  return out;
}

std::string systemLine() {
  auto sys = sysinfo::collect();
  if (!sys.ok()) return std::string("system info unavailable: ") + sys.error().message;
  const auto& s = sys.value();
  std::ostringstream o;
  o << s.osVersion << " | " << s.cpuModel << " x" << s.logicalProcessors;
  if (s.ramMB > 0) o << " | " << s.ramMB << " MB RAM";
  if (!s.windowsBuild.empty()) o << " (build " << s.windowsBuild << ")";
  return o.str();
}

std::string gamepadLine() {
  auto b = input::windowsBackend();
  if (!b.ok()) return std::string("input backend unavailable: ") + b.error().message;
  auto g = b.value()->enumerateGamepads();
  if (!g.ok())
    return std::string("gamepad enumeration: ") + g.error().message +
           " (capability skip on non-Windows)";
  if (g.value().empty()) return "no gamepads connected (XInput poll, 0/4 slots)";
  std::ostringstream o;
  o << g.value().size() << " gamepad(s): ";
  bool first = true;
  for (const auto& p : g.value()) {
    if (!first) o << ", ";
    first = false;
    o << p.id << " [slot " << p.index << "]";
  }
  return o.str();
}

std::string audioLine() {
  auto b = audio::windowsBackend();
  if (!b.ok()) return std::string("audio backend unavailable: ") + b.error().message;
  auto d = b.value()->enumerate();
  if (!d.ok())
    return std::string("audio enumeration: ") + d.error().message +
           " (capability skip on non-Windows)";
  std::ostringstream o;
  o << d.value().size() << " device(s): ";
  bool first = true;
  for (const auto& dev : d.value()) {
    if (!first) o << ", ";
    first = false;
    o << dev.name;
  }
  o << " (test tone Unsupported until streaming lands)";
  return o.str();
}

std::string adapterLine() {
  auto b = gfx::d3d12Backend();
  if (!b.ok())
    return std::string("D3D12 unavailable: ") + b.error().message +
           " (capability skip — run sb_gpu_probe; exit 77 where D3D12 is absent)";
  auto a = b.value()->enumerateAdapters(false);
  if (!a.ok())
    return std::string("adapter enumeration failed: ") + a.error().message;
  if (a.value().empty()) return "no adapters reported";
  std::ostringstream o;
  o << a.value().size() << " adapter(s): ";
  bool first = true;
  for (const auto& ad : a.value()) {
    if (!first) o << "; ";
    first = false;
    o << ad.description << (ad.isSoftware ? " [software]" : " [hardware]");
  }
  return o.str();
}

std::string cacheFingerprint() { return cache::translatorFingerprint(); }

CacheStats queryCacheStats(const std::string& dir, std::size_t maxEntries,
                           std::size_t maxBytes) {
  CacheStats out;
  out.dir = dir;
  out.maxEntries = maxEntries;
  out.maxBytes = maxBytes;
  cache::ShaderCache c(dir, maxEntries, maxBytes);
  std::size_t entries = 0, bytes = 0;
  if (c.stats(entries, bytes).ok()) {
    out.entries = entries;
    out.bytes = bytes;
  }
  out.overBudget = out.entries >= maxEntries || out.bytes > maxBytes;
  return out;
}

std::string cacheStatsLine(const CacheStats& s) {
  std::ostringstream o;
  o << s.entries << (s.entries == 1 ? " entry, " : " entries, ") << s.bytes
    << " bytes (budget " << s.maxEntries
    << (s.maxEntries == 1 ? " entry / " : " entries / ") << s.maxBytes / (1024 * 1024)
    << " MB) in " << s.dir
    << (s.overBudget ? " — BUDGET EXHAUSTED, stores refuse" : " — within budget");
  return o.str();
}

std::string smokeRateLine(double drawMs) {
  // 1000/drawMs overflows to +inf for drawMs == 0 and stays NaN for NaN;
  // tenthsFromFps rejects non-finite and negative rates, so every unusable
  // draw time lands here without a throw or a faked rate.
  const auto tenths = diag::tenthsFromFps(1000.0 / drawMs);
  if (!tenths) return "no frame-rate sample (smoke produced no draw time)";
  return diag::frameRateMsText(*tenths) +
         " (single smoke sample — not a sustained stream)";
}

std::string smokeBottleneckLine() {
  return std::string("bottleneck: ") + diag::bottleneckName(diag::Bottleneck::Unknown) +
         " (one-shot smoke has no continuous CPU/GPU stream)";
}

std::string scriptSummaryLine(const input::InputScript& script) {
  if (script.items == 0) return "script: empty (no items)";
  std::ostringstream o;
  o << "script: " << script.entries.size()
    << (script.entries.size() == 1 ? " entry, " : " entries, ") << script.rejections.size()
    << (script.rejections.size() == 1 ? " rejection, " : " rejections, ")
    << "base=" << (script.fromProcessStart ? "start" : "poll");
  if (!script.entries.empty()) {
    o << ", span " << std::fixed << std::setprecision(1)
      << ::sb::input::firstInputStart(script) << ".." << std::fixed << std::setprecision(1)
      << ::sb::input::lastInputEnd(script) << " s";
  }
  return o.str();
}

std::string scriptStateLine(const input::ScriptState& state) {
  std::ostringstream o;
  o << "pad: buttons=0x" << std::hex << std::uppercase << state.buttons << std::dec;
  o << " axes lt=" << std::fixed << std::setprecision(2) << state.axes[0]
    << " rt=" << std::fixed << std::setprecision(2) << state.axes[1]
    << " lx=" << std::fixed << std::setprecision(2) << state.axes[2]
    << " ly=" << std::fixed << std::setprecision(2) << state.axes[3]
    << " rx=" << std::fixed << std::setprecision(2) << state.axes[4]
    << " ry=" << std::fixed << std::setprecision(2) << state.axes[5];
  return o.str();
}

std::vector<std::string> scriptDetailLines(const input::InputScript& script,
                                            std::size_t maxDetail) {
  std::vector<std::string> out;
  try {
    for (const auto& r : script.rejections) {
      if (out.size() >= maxDetail) break;
      std::string text = r.text;
      if (text.size() > 80) text = text.substr(0, 77) + "...";
      out.push_back("item " + std::to_string(r.index) + ": " + r.reason + ": " + text);
    }
  } catch (...) {
    out.clear();
  }
  return out;
}

std::string engineInstanceLine(const std::string& name) {
  const auto v = diag::parseEngineInstance(name);
  if (!v) return "not an engine instance name (want pid_<pid>_..._engtype_<kind>)";
  std::ostringstream o;
  o << "pid=" << v->first << " kind=" << (v->second.empty() ? "<empty>" : v->second);
  return o.str();
}

std::string folderReportText(const FolderScan& scan) {
  try {
    std::ostringstream o;
    o << "scarlettbridge folder-scan report\n";
    o << "dir: " << scan.shownDir << "\n";
    o << "entries: " << scan.entries << "\n";
    o << "bytes: " << scan.bytes << "\n";
    o << "hashed: " << scan.hashed << "\n";
    o << "scan_ms: " << std::fixed << std::setprecision(1) << scan.ms << "\n";
    if (scan.ok) {
      o << "status: ok\n";
    } else {
      o << "status: failed: " << scan.error << "\n";
    }
    return o.str();
  } catch (...) {
    return {};
  }
}

std::vector<std::string> settingBackends() { return {"d3d12"}; }
std::vector<std::string> settingWindowModes() { return {"windowed", "borderless", "fullscreen"}; }
std::vector<std::string> settingResolutions() {
  return {"1280x720", "1920x1080", "2560x1440", "3840x2160"};
}
bool isValidBackend(const std::string& v) {
  for (const auto& o : settingBackends())
    if (o == v) return true;
  return false;
}
bool isValidWindowMode(const std::string& v) {
  for (const auto& o : settingWindowModes())
    if (o == v) return true;
  return false;
}
bool isValidResolution(const std::string& v) {
  for (const auto& o : settingResolutions())
    if (o == v) return true;
  return false;
}
std::string settingsSummary(const Settings& s) {
  std::ostringstream o;
  o << s.backend << " | " << s.resolution << " " << s.windowMode
    << " vsync=" << (s.vsync ? "on" : "off") << " cap=" << s.frameCap
    << " tex=" << s.texBudgetMB << "MB cache=" << (s.shaderCache ? "on" : "off") << "("
    << s.cacheMB << "MB) precompile=" << (s.precompile ? "on" : "off")
    << " debug=" << (s.debugLayer ? "on" : "off") << " dred=" << (s.dred ? "on" : "off")
    << " adapter=" << s.adapter;
  return o.str();
}

std::string redactLine(const std::string& s) { return log::redactPaths(s); }

bool matchesFilter(const std::string& haystack, const std::string& needle) {
  if (needle.empty()) return true;
  std::string h = haystack, n = needle;
  for (char& c : h) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (char& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return h.find(n) != std::string::npos;
}

std::string diagnosticsJson(const std::vector<std::pair<std::string, double>>& counters,
                            const std::vector<diag::TraceEvent>& trace) {
  json::Value v = json::Value::object();
  v.set("app", json::Value::str(kAppVersion));
  v.set("build", json::Value::str(kBuildId));
  v.set("ipc", json::Value::number(kIpcProtocolVersion));
  v.set("schema", json::Value::number(kConfigSchemaVersion));
  json::Value c = json::Value::object();
  for (const auto& kv : counters) c.set(kv.first, json::Value::number(kv.second));
  v.set("counters", std::move(c));
  json::Value t = json::Value::array();
  for (const auto& e : trace) {
    json::Value o = json::Value::object();
    o.set("t", json::Value::str(e.t));
    o.set("sys", json::Value::str(e.subsystem));
    o.set("msg", json::Value::str(log::redactPaths(e.message)));
    t.push(std::move(o));
  }
  v.set("trace", std::move(t));
  return v.serialize();
}

Result<void> exportTextOutsideRepo(const std::string& path, const std::string& text) {
  if (path.empty())
    return Result<void>::fail(Error::make("export", 1, "empty export path"));
  if (auto g = fs::ensureOutsideRepo(path); !g.ok())
    return Result<void>::fail(Error::make("export", 2, g.error().message));
  std::error_code ec;
  std::filesystem::path p(path);
  if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) return Result<void>::fail(Error::make("export", 3, "cannot open export file"));
  f << text;
  if (!f) return Result<void>::fail(Error::make("export", 4, "export write failed"));
  return Result<void>::ok();
}

std::string nullCensusLine(const diag::NullCensus& c) {
  std::ostringstream o;
  o << "null bindings total=" << c.total() << " (entry="
    << c.counts[static_cast<unsigned>(diag::NullCause::Entry)] << " buffer="
    << c.counts[static_cast<unsigned>(diag::NullCause::Buffer)] << " token="
    << c.counts[static_cast<unsigned>(diag::NullCause::Token)] << " wrapper="
    << c.counts[static_cast<unsigned>(diag::NullCause::Wrapper)] << " dimension="
    << c.counts[static_cast<unsigned>(diag::NullCause::Dimension)] << ")";
  return o.str();
}

std::string perReasonCensusLine(const diag::PerReasonCensus& census) {
  std::ostringstream o;
  o << "refusals total=" << census.total() << " distinct keys=" << census.keys()
    << " distinct pairs=" << census.entries() << " overflow=" << census.overflow();
  return o.str();
}

std::vector<std::string> perReasonDetailLines(diag::PerReasonCensus& census,
                                              std::size_t maxDetail) {
  std::vector<std::string> out;
  try {
    std::vector<std::string> full = census.report(false);
    if (full.empty()) return out;
    out.push_back(full[0]);
    for (std::size_t i = 1; i < full.size() && out.size() <= maxDetail; ++i)
      out.push_back(full[i]);
  } catch (...) {
    out.clear();
  }
  return out;
}

std::string failTallyLine(const gfx::FailTally& tally) {
  try {
    std::ostringstream o;
    o << "shader failures total=" << tally.total() << " reasons=" << tally.reasons();
    return o.str();
  } catch (...) {
    return {};
  }
}

std::vector<std::string> failTallyReportLines(const gfx::FailTally& tally,
                                              std::size_t maxDetail) {
  std::vector<std::string> out;
  try {
    out.push_back(failTallyLine(tally));
    for (const auto& line : tally.report()) {
      if (out.size() > maxDetail) break;
      out.push_back(line);
    }
  } catch (...) {
    out.clear();
  }
  return out;
}

}  // namespace ui
}  // namespace sb
