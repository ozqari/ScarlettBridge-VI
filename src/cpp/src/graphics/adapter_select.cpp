#include "graphics/adapter_select.h"

#include <cctype>

namespace sb {
namespace gfx {
namespace {

std::string lowerStr(const std::string& s) {
  std::string out = s;
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

bool blankChar(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

std::string trimStr(const std::string& s) {
  std::size_t b = 0, e = s.size();
  while (b < e && blankChar(s[b])) ++b;
  while (e > b && blankChar(s[e - 1])) --e;
  return s.substr(b, e - b);
}

bool allDigits(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s)
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  return true;
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

bool isWarpRequest(const std::string& filter) { return lowerStr(filter) == "warp"; }

AdapterSelection parseAdapterSelector(const std::string& filter) {
  const std::string trimmed = trimStr(filter);
  const std::string folded = lowerStr(trimmed);
  AdapterSelection out;
  if (folded.empty() || folded == "auto") {
    out.kind = AdapterSelector::Auto;
    return out;
  }
  if (folded == "warp") {
    out.kind = AdapterSelector::Warp;
    return out;
  }
  // A plain number ("0", but not "007" or "+1") is a hardware ordinal.
  // Values too large to hold fall back to a name, like a failed parse.
  if (allDigits(trimmed) && (trimmed == "0" || trimmed[0] != '0')) {
    std::size_t n = 0;
    bool overflow = false;
    for (char c : trimmed) {
      const std::size_t d = static_cast<std::size_t>(c - '0');
      if (n > (static_cast<std::size_t>(-1) - d) / 10) {
        overflow = true;
        break;
      }
      n = n * 10 + d;
    }
    if (!overflow) {
      out.kind = AdapterSelector::Index;
      out.index = n;
      return out;
    }
  }
  out.kind = AdapterSelector::Name;
  out.name = folded;
  return out;
}

bool containsNumericToken(const std::string& haystack, const std::string& digits) {
  if (digits.empty()) return false;
  std::size_t from = 0;
  while (from < haystack.size()) {
    const std::size_t pos = haystack.find(digits, from);
    if (pos == std::string::npos) return false;
    const std::size_t end = pos + digits.size();
    const bool beforeOk = pos == 0 || !isDigit(haystack[pos - 1]);
    const bool afterOk = end == haystack.size() || !isDigit(haystack[end]);
    if (beforeOk && afterOk) return true;
    from = end;
  }
  return false;
}

std::vector<int> selectAdapterIndices(const std::vector<AdapterInfo>& adapters,
                                      const AdapterSelection& sel) {
  std::vector<int> out;
  switch (sel.kind) {
    case AdapterSelector::Auto:
      for (std::size_t i = 0; i < adapters.size(); ++i)
        if (!adapters[i].isSoftware) out.push_back(static_cast<int>(i));
      break;
    case AdapterSelector::Warp:
      for (std::size_t i = 0; i < adapters.size(); ++i)
        if (adapters[i].isSoftware) out.push_back(static_cast<int>(i));
      break;
    case AdapterSelector::Name:
      for (std::size_t i = 0; i < adapters.size(); ++i)
        if (lowerStr(adapters[i].description).find(sel.name) != std::string::npos)
          out.push_back(static_cast<int>(i));
      break;
    case AdapterSelector::Index: {
      std::vector<int> hardware;
      for (std::size_t i = 0; i < adapters.size(); ++i)
        if (!adapters[i].isSoftware) hardware.push_back(static_cast<int>(i));
      if (sel.index < hardware.size()) {
        out.push_back(hardware[sel.index]);
        break;
      }
      // Out of range: match the number as a standalone numeric model token.
      const std::string token = std::to_string(sel.index);
      for (int h : hardware)
        if (containsNumericToken(adapters[static_cast<std::size_t>(h)].description, token))
          out.push_back(h);
      break;
    }
  }
  return out;
}

std::string describeSelector(const AdapterSelection& sel) {
  switch (sel.kind) {
    case AdapterSelector::Auto:
      return "auto: first hardware adapter in high-performance order";
    case AdapterSelector::Warp:
      return "warp: software renderer (WARP)";
    case AdapterSelector::Index: {
      const std::string n = std::to_string(sel.index);
      return "adapter " + n + ": " + n +
             "-th hardware adapter in high-performance order (0 = first); out of range "
             "falls back to model token \"" +
             n + "\"";
    }
    case AdapterSelector::Name:
      return "name \"" + sel.name +
             "\": adapter whose description contains it (case-insensitive)";
  }
  return "?";
}

int chooseAdapter(const std::vector<AdapterInfo>& adapters, const std::string& filter,
                  bool fallbackWarp) {
  if (adapters.empty()) return -1;
  const AdapterSelection sel = parseAdapterSelector(filter);
  const std::vector<int> hits = selectAdapterIndices(adapters, sel);
  if (!hits.empty()) return hits.front();
  if (sel.kind == AdapterSelector::Auto) return 0;  // no hardware: historical index 0
  if (fallbackWarp) {
    for (std::size_t i = 0; i < adapters.size(); ++i)
      if (adapters[i].isSoftware) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace gfx
}  // namespace sb
