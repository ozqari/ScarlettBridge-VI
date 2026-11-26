#pragma once
// Structured logging: severity + timestamp + subsystem + message (+ optional
// correlation id). Console + rotating file sinks. Thread-safe. No dependencies.
#include <cstdint>
#include <string>

namespace sb {
namespace log {

enum class Severity { Debug = 0, Info = 1, Warn = 2, Error = 3 };

struct Record {
  std::string timestamp;  // ISO-8601 UTC
  Severity severity;
  std::string subsystem;  // "ipc", "runtime", "graphics", "shader", ...
  std::string message;
  std::string correlation;  // optional run/crash id
};

void init(const std::string& logDir, Severity minConsole, Severity minFile);
void shutdown();
void setMinConsole(Severity s);
void write(Severity sev, const std::string& subsystem, const std::string& message,
           const std::string& correlation = "");
inline void debug(const std::string& s, const std::string& m) { write(Severity::Debug, s, m); }
inline void info(const std::string& s, const std::string& m) { write(Severity::Info, s, m); }
inline void warn(const std::string& s, const std::string& m) { write(Severity::Warn, s, m); }
inline void error(const std::string& s, const std::string& m) { write(Severity::Error, s, m); }

// Redacts Windows user paths for shared reports. Pure function, testable.
std::string redactPaths(const std::string& text);

}  // namespace log
}  // namespace sb
