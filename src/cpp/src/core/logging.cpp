#include "core/logging.h"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace sb {
namespace log {
namespace {
std::mutex g_mu;
bool g_init = false;
std::string g_dir;
Severity g_minConsole = Severity::Info;
Severity g_minFile = Severity::Debug;
std::ofstream g_file;
const char* kName[] = {"DEBUG", "INFO", "WARN", "ERROR"};

std::string utcNow() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900,
                tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  return std::string(buf);
}
}  // namespace

void init(const std::string& logDir, Severity minConsole, Severity minFile) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_dir = logDir;
  g_minConsole = minConsole;
  g_minFile = minFile;
  if (!g_dir.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(g_dir, ec);
    if (!ec) {
      g_file.open(std::filesystem::path(g_dir) / "scarlettbridge.log",
                  std::ios::app);
      g_init = g_file.is_open();
      return;
    }
  }
  g_init = false;
}

void shutdown() {
  std::lock_guard<std::mutex> lock(g_mu);
  if (g_file.is_open()) g_file.close();
  g_init = false;
}

void setMinConsole(Severity s) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_minConsole = s;
}

void write(Severity sev, const std::string& subsystem, const std::string& message,
           const std::string& correlation) {
  std::lock_guard<std::mutex> lock(g_mu);
  std::string line = "[" + utcNow() + " " + kName[static_cast<int>(sev)] + "/" +
                     subsystem + "]" + (correlation.empty() ? "" : " (" + correlation + ")") +
                     " " + message;
  if (sev >= g_minConsole) {
    (sev >= Severity::Warn ? std::fprintf(stderr, "%s\n", line.c_str())
                           : std::fprintf(stdout, "%s\n", line.c_str()));
  }
  if (g_file.is_open() && sev >= g_minFile) {
    g_file << line << "\n";
    g_file.flush();
  }
}

std::string redactPaths(const std::string& text) {
  // Redact drive-letter paths (C:\Users\name\...) and POSIX home dirs.
  std::string out = text;
  for (std::size_t i = 0; i + 3 < out.size(); ++i) {
    bool drive = ((out[i] >= 'A' && out[i] <= 'Z') || (out[i] >= 'a' && out[i] <= 'z')) &&
                 out[i + 1] == ':' && (out[i + 2] == '\\' || out[i + 2] == '/');
    if (drive) {
      std::size_t end = i + 3;
      while (end < out.size() && out[end] != ' ' && out[end] != '"' && out[end] != '\n')
        ++end;
      out.replace(i, end - i, "<redacted>");
    }
  }
  for (const char* home : {"/home/", "/Users/"}) {
    std::size_t at = 0;
    while ((at = out.find(home, at)) != std::string::npos) {
      std::size_t end = at;
      while (end < out.size() && out[end] != ' ' && out[end] != '"' && out[end] != '\n')
        ++end;
      out.replace(at, end - at, "<redacted>");
      at += 10;
    }
  }
  return out;
}

}  // namespace log
}  // namespace sb
