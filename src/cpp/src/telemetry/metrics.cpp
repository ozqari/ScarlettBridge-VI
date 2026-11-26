#include "telemetry/metrics.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "core/filesystem_guards.h"
#include "core/json.h"
#include "core/logging.h"

namespace sb {
namespace diag {

void Counters::add(const std::string& name, double value) {
  for (auto& kv : items_)
    if (kv.first == name) {
      kv.second += value;
      return;
    }
  items_.emplace_back(name, value);
}

double Counters::get(const std::string& name, double fallback) const {
  for (const auto& kv : items_)
    if (kv.first == name) return kv.second;
  return fallback;
}

std::vector<std::pair<std::string, double>> Counters::snapshot() const { return items_; }

void TraceRing::push(TraceEvent e) {
  if (buf_.size() < cap_) {
    buf_.push_back(std::move(e));
    return;
  }
  buf_[head_] = std::move(e);
  head_ = (head_ + 1) % cap_;
}

std::vector<TraceEvent> TraceRing::snapshot() const {
  if (buf_.size() < cap_) return buf_;
  std::vector<TraceEvent> out;
  out.reserve(buf_.size());
  for (std::size_t i = 0; i < buf_.size(); ++i) out.push_back(buf_[(head_ + i) % buf_.size()]);
  return out;
}

std::string CrashReport::toJson(bool redact) const {
  json::Value v = json::Value::object();
  v.set("id", json::Value::str(id));
  v.set("t", json::Value::str(timestamp));
  v.set("reason", json::Value::str(reason));
  v.set("runtime", json::Value::str(runtimeVersion));
  json::Value ev = json::Value::array();
  for (const auto& e : lastEvents)
    ev.push(json::Value::str(redact ? log::redactPaths(e) : e));
  v.set("lastEvents", std::move(ev));
  return v.serialize();
}

std::string formatFaultReport(unsigned long exceptionCode, const void* faultAddress,
                              const std::string& moduleHint) {
  char buf[256];
  std::snprintf(buf, sizeof(buf), "fault code=0x%08lX addr=%p module=%s", exceptionCode,
                faultAddress, moduleHint.empty() ? "?" : moduleHint.c_str());
  return std::string(buf);
}

std::string formatFaultReportWithBase(unsigned long exceptionCode, const void* faultAddress,
                                      const void* moduleBase,
                                      const std::string& moduleHint) {
  const char* mod = moduleHint.empty() ? "?" : moduleHint.c_str();
  if (moduleBase == nullptr || faultAddress < moduleBase) {
    return formatFaultReport(exceptionCode, faultAddress, moduleHint);
  }
  std::uintptr_t rva = static_cast<const unsigned char*>(faultAddress) -
                       static_cast<const unsigned char*>(moduleBase);
  char buf[320];
  std::snprintf(buf, sizeof(buf), "fault code=0x%08lX addr=%p module=%s+0x%llX", exceptionCode,
                faultAddress, mod, static_cast<unsigned long long>(rva));
  return std::string(buf);
}

void LastOutputRing::push(const std::string& line) {
  if (line.empty() || cap_ == 0) return;
  if (buf_.size() < cap_) {
    buf_.push_back(line);
    return;
  }
  buf_.erase(buf_.begin());
  buf_.push_back(line);
}

std::vector<std::string> LastOutputRing::snapshot() const { return buf_; }

bool writeCrashLog(const std::string& filePath, const CrashReport& report) {
  if (filePath.empty()) return false;
  // Crash dumps are game-derived: refuse in-tree destinations.
  if (auto g = sb::fs::ensureOutsideRepo(filePath); !g.ok()) return false;
  std::error_code ec;
  std::filesystem::path p(filePath);
  if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::app);
  if (!f) return false;
  std::uint64_t unix = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
#if defined(_WIN32)
  unsigned long pid = static_cast<unsigned long>(::GetCurrentProcessId());
#else
  unsigned long pid = 0;
#endif
  f << "unix time " << unix << ", process " << pid << "\n" << report.toJson(true) << "\n";
  return !!f;
}

}  // namespace diag
}  // namespace sb
