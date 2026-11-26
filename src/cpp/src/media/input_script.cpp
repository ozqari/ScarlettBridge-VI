#include "media/input_script.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace sb {
namespace input {
namespace {

bool blankChar(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

std::string trimStr(const std::string& s) {
  std::size_t b = 0, e = s.size();
  while (b < e && blankChar(s[b])) ++b;
  while (e > b && blankChar(s[e - 1])) --e;
  return s.substr(b, e - b);
}

bool startsWith(const std::string& s, const char* word) {
  const std::size_t n = std::strlen(word);
  return s.size() >= n && s.compare(0, n, word) == 0;
}

// "@<seconds>[+<duration>]" at `p`, or "@+<seconds>[+<duration>]" after
// `cursor`. Returns the failure reason, or nullptr with entry->at/duration
// filled. Never throws.
const char* parseWhen(const char* p, double cursor, ScriptEntry& entry) {
  if (*p != '@') return "missing @<seconds>";
  const bool relative = p[1] == '+';
  p += relative ? 2 : 1;
  char* stop = nullptr;
  entry.at = std::strtod(p, &stop);
  if (stop == p || (relative && !(entry.at >= 0.0))) return "bad time";
  if (relative) entry.at += cursor;
  if (!std::isfinite(entry.at)) return "bad time";
  p = stop;
  entry.duration = kDefaultHoldSeconds;
  if (*p == '+') {
    entry.duration = std::strtod(p + 1, &stop);
    if (stop == p + 1 || !std::isfinite(entry.duration) || !(entry.duration > 0.0))
      return "bad duration";
    p = stop;
  }
  if (*p) return "trailing text";
  return nullptr;
}

int axisByName(const std::string& name) {
  if (name == "lt") return 0;
  if (name == "rt") return 1;
  if (name == "lx") return 2;
  if (name == "ly") return 3;
  if (name == "rx") return 4;
  if (name == "ry") return 5;
  return -1;
}

// wait/mark/group/end take a picture-signal presenter, which this portable
// subset has none of — name the boundary instead of running them.
bool isUnsupportedDirective(const std::string& item) {
  if (item == "wait" || item == "mark" || item == "group" || item == "end") return true;
  for (const char* w : {"wait", "mark"}) {
    const std::size_t n = std::strlen(w);
    if (item.size() > n && item.compare(0, n, w) == 0 &&
        (item[n] == '=' || item[n] == '@' || (w[0] == 'w' && item[n] == ';')))
      return true;
  }
  if (item.size() > 5 && startsWith(item, "group") && (item[5] == '=' || item[5] == '@'))
    return true;
  if (startsWith(item, "end@")) return true;
  return false;
}

}  // namespace

InputScript parseInputScript(const std::string& text) {
  InputScript out;
  std::size_t index = 0;
  double cursor = 0.0;
  std::size_t pos = 0;
  for (;;) {
    const std::size_t comma = text.find(',', pos);
    const std::string item = trimStr(comma == std::string::npos
                                         ? text.substr(pos)
                                         : text.substr(pos, comma - pos));
    if (!item.empty()) {
      ++index;
      out.items = index;
      const char* reason = nullptr;
      ScriptEntry entry;
      bool directive = false;
      if (isUnsupportedDirective(item)) {
        reason = "unsupported directive (waits, marks and groups need a presenter)";
      } else {
        const std::size_t eq = item.find('=');
        const std::size_t at = item.find('@');
        if (eq != std::string::npos && (at == std::string::npos || eq < at)) {
          std::string name = trimStr(item.substr(0, eq));
          std::string value = item.substr(eq + 1);
          std::size_t vb = 0;
          while (vb < value.size() && blankChar(value[vb])) ++vb;
          value.erase(0, vb);
          if (name == "base") {
            if (index != 1) {
              reason = "base= must be the first item";
            } else if (value != "poll" && value != "start") {
              reason = "unknown base (use poll or start)";
            } else {
              out.fromProcessStart = value == "start";
              directive = true;
            }
          } else {
            const int axis = axisByName(name);
            if (axis < 0) {
              reason = "unknown token";
            } else {
              char* stop = nullptr;
              const double v = std::strtod(value.c_str(), &stop);
              if (stop == value.c_str() || !std::isfinite(v)) {
                reason = "bad value";
              } else if (v > 1.0 || v < ((axis <= 1) ? 0.0 : -1.0)) {
                reason = (axis <= 1) ? "value out of range [0, 1]"
                                     : "value out of range [-1, 1]";
              } else {
                entry.axis = axis;
                entry.value = static_cast<float>(v);
                reason = parseWhen(stop, cursor, entry);
              }
            }
          }
        } else {
          // Button mask in hex (strtoull and strtod accept what the old
          // sscanf "%x@%lf" form accepted, 0x-prefix included).
          errno = 0;
          char* stop = nullptr;
          const unsigned long long mask =
              std::strtoull(item.c_str(), &stop, 16);
          if (stop == item.c_str() || errno == ERANGE ||
              (mask > 0xFFFFFFFFull && mask < 0xFFFFFFFF00000000ull)) {
            reason = "bad mask";
          } else {
            entry.axis = -1;
            entry.mask = static_cast<std::uint32_t>(mask);
            reason = parseWhen(stop, cursor, entry);
          }
        }
      }
      if (reason != nullptr) {
        out.rejections.push_back({index, reason, item});
      } else if (directive) {
        // base=: counted above, holds no entry.
      } else if (out.entries.size() >= kMaxScriptEntries) {
        out.rejections.push_back({index, "over 256 entries", item});
      } else {
        entry.item = index;
        out.entries.push_back(entry);
        cursor = entry.at;
      }
    }
    if (comma == std::string::npos) break;
    pos = comma + 1;
  }
  return out;
}

ScriptState playInputScript(const InputScript& script, double t) {
  ScriptState out;
  for (const ScriptEntry& e : script.entries) {
    if (!(t >= e.at && t < e.at + e.duration)) continue;
    if (e.axis < 0) {
      out.buttons |= e.mask;
    } else {
      const std::size_t a = static_cast<std::size_t>(e.axis);
      out.axes[a] = e.value;
      out.active[a] = true;
    }
  }
  return out;
}

double firstInputStart(const InputScript& script) {
  double first = 0.0;
  bool any = false;
  for (const ScriptEntry& e : script.entries) {
    if (!any || e.at < first) {
      first = e.at;
      any = true;
    }
  }
  return any ? first : 0.0;
}

double lastInputEnd(const InputScript& script) {
  double last = 0.0;
  for (const ScriptEntry& e : script.entries) {
    const double end = e.at + e.duration;
    if (end > last) last = end;
  }
  return last;
}

}  // namespace input
}  // namespace sb
