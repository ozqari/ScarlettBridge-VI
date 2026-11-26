#pragma once
// Diagnostics: monotonic counters, a bounded trace ring, and crash-report
// assembly with path redaction. Device-loss reasons flow through here to the
// UI and to exported reports.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {
namespace diag {

class Counters {
 public:
  void add(const std::string& name, double value);
  double get(const std::string& name, double fallback = 0.0) const;
  std::vector<std::pair<std::string, double>> snapshot() const;

 private:
  std::vector<std::pair<std::string, double>> items_;
};

struct TraceEvent {
  std::string t;
  std::string subsystem;
  std::string message;
};

class TraceRing {
 public:
  explicit TraceRing(std::size_t capacity = 4096) : cap_(capacity) {}
  void push(TraceEvent e);
  std::vector<TraceEvent> snapshot() const;

 private:
  std::size_t cap_;
  std::vector<TraceEvent> buf_;
  std::size_t head_ = 0;
};

struct CrashReport {
  std::string id;
  std::string timestamp;
  std::string reason;
  std::string runtimeVersion;
  std::vector<std::string> lastEvents;
  std::string toJson(bool redactPaths) const;
};

// Keeps the last N lines of title/debug output for the next crash report.
// Idea from victory-loader crash.rs (SAID ring, 8 lines): often states the
// reason. Bounded, drops the oldest line when full, never blocks.
class LastOutputRing {
 public:
  explicit LastOutputRing(std::size_t capacity = 8) : cap_(capacity) {}
  void push(const std::string& line);
  std::vector<std::string> snapshot() const;

 private:
  std::size_t cap_;
  std::vector<std::string> buf_;
};

// Names a fault the way loader crash reporters do: module+RVA style detail
// plus the raw code/address. Pure function, unit-tested.
std::string formatFaultReport(unsigned long exceptionCode, const void* faultAddress,
                              const std::string& moduleHint);
// Same, with an explicit module base so the report names module+RVA
// (e.g. "title.exe+0x1234") instead of only the absolute address.
std::string formatFaultReportWithBase(unsigned long exceptionCode, const void* faultAddress,
                                      const void* moduleBase, const std::string& moduleHint);
// Appends a crash report to a log file with unix time + process id, creating
// parent directories as needed. Returns false on I/O failure (never throws).
bool writeCrashLog(const std::string& filePath, const CrashReport& report);

}  // namespace diag
}  // namespace sb
