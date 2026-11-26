#include "telemetry/trace_filter.h"

#include <cctype>
#include <sstream>

namespace sb {
namespace diag {
namespace {

bool startsWith(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string trimLower(std::string s) {
  std::string out;
  for (char c : s) {
    if (c == ' ' || c == '\t') continue;
    out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

}  // namespace

bool FirstSeenThrottle::first(const std::string& key) {
  for (const auto& k : seen_)
    if (k == key) return false;
  if (seen_.size() >= cap_) return false;  // table full: stay silent
  seen_.push_back(key);
  return true;
}

bool FirstSeenThrottle::first(const std::string& a, const std::string& b) {
  return first(a + '\x1f' + b);
}

std::string TraceFilter::lower(const std::string& s) {
  std::string out = s;
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

TraceFilter TraceFilter::parse(const std::string& env) {
  TraceFilter f;
  f.hle = false;
  f.all = false;
  std::stringstream ss(env);
  std::string word;
  while (std::getline(ss, word, ',')) {
    std::string w = trimLower(word);
    if (w.empty()) continue;
    if (w == "1" || w == "hle") {
      f.hle = true;
    } else if (w == "all") {
      f.all = true;
    } else if (w[0] == '-' && w.size() > 1) {
      f.exclude.push_back(w.substr(1));
    } else {
      f.include.push_back(w);
    }
  }
  if (!f.all && f.include.empty()) f.hle = true;  // default: trace HLE
  return f;
}

bool TraceFilter::wants(const std::string& dll, const std::string& function,
                        TraceSource source) const {
  if (source == TraceSource::Stub) return false;  // stubs log their own first call
  std::string full = lower(dll) + "!" + lower(function);
  for (const auto& x : exclude)
    if (startsWith(full, x)) return false;
  if (source == TraceSource::Hle || source == TraceSource::HleOther) {
    if (hle) return true;
  } else {
    if (all) return true;
  }
  for (const auto& inc : include)
    if (startsWith(full, inc)) return true;
  return false;
}

}  // namespace diag
}  // namespace sb
