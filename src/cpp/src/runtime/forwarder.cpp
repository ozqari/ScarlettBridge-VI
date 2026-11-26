#include "runtime/forwarder.h"

#include <cctype>

namespace sb {
namespace {

std::string lowerStr(const std::string& s) {
  std::string o = s;
  for (char& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return o;
}

bool endsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

std::optional<ForwarderTarget> parseForwarderSpec(const std::string& spec) {
  std::size_t dot = spec.rfind('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= spec.size()) return std::nullopt;
  std::string dll = spec.substr(0, dot);
  std::string func = spec.substr(dot + 1);
  if (dll.empty() || func.empty()) return std::nullopt;
  ForwarderTarget t;
  t.dllKey = lowerStr(dll);
  if (!endsWith(t.dllKey, ".dll")) t.dllKey += ".dll";
  if (func[0] == '#') {
    if (func.size() == 1) return std::nullopt;
    unsigned long v = 0;
    for (std::size_t i = 1; i < func.size(); ++i) {
      if (func[i] < '0' || func[i] > '9') return std::nullopt;
      v = v * 10 + static_cast<unsigned long>(func[i] - '0');
      if (v > 65535) return std::nullopt;
    }
    t.isOrdinal = true;
    t.ordinal = static_cast<std::uint16_t>(v);
  } else {
    t.name = func;
  }
  return t;
}

std::optional<ForwardResolveResult> resolveForwarderChain(const std::string& spec,
                                                        const ExportLookup& lookup) {
  std::string cur = spec;
  for (int depth = 0; depth <= kMaxForwarderDepth; ++depth) {
    auto t = parseForwarderSpec(cur);
    if (!t.has_value()) return std::nullopt;
    std::string sym = t->isOrdinal ? ("#" + std::to_string(t->ordinal)) : t->name;
    auto out = lookup(t->dllKey, sym);
    if (!out.has_value()) return std::nullopt;  // missing: null, never a stub
    if (!out->isForwarder) return ForwardResolveResult{t->dllKey, out->rva};
    if (depth == kMaxForwarderDepth) return std::nullopt;
    cur = out->forwarder;
  }
  return std::nullopt;
}

}  // namespace sb
