#include "runtime/init_order.h"

#include <cctype>
#include <functional>
#include <map>

namespace sb {
namespace {

std::string lowerStr(const std::string& s) {
  std::string o = s;
  for (char& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return o;
}

}  // namespace

std::string toString(Binding b) {
  switch (b) {
    case Binding::Shim: return "shim";
    case Binding::Host: return "host";
    case Binding::Module: return "module";
    case Binding::Unimplemented: return "unimplemented";
  }
  return "?";
}

Binding bindingFromSource(ResolvedSource s) {
  switch (s) {
    case ResolvedSource::Hle:
    case ResolvedSource::HleOther: return Binding::Shim;
    case ResolvedSource::System: return Binding::Host;
    case ResolvedSource::GameDll: return Binding::Module;
    case ResolvedSource::Stub: return Binding::Unimplemented;
  }
  return Binding::Unimplemented;
}

BindingCounts countBindings(const std::vector<ResolvedSource>& sources) {
  BindingCounts c;
  for (auto s : sources) {
    switch (bindingFromSource(s)) {
      case Binding::Shim: ++c.shim; break;
      case Binding::Host: ++c.host; break;
      case Binding::Module: ++c.module; break;
      case Binding::Unimplemented: ++c.unimplemented; break;
    }
  }
  return c;
}

std::vector<std::string> computeInitOrder(const std::vector<ModuleNode>& mappedOrder) {
  std::map<std::string, std::size_t> index;  // lower name -> position
  for (std::size_t i = 0; i < mappedOrder.size(); ++i)
    index[lowerStr(mappedOrder[i].name)] = i;
  std::vector<char> done(mappedOrder.size(), 0);
  std::vector<char> stack(mappedOrder.size(), 0);
  std::vector<std::string> out;
  std::function<void(std::size_t)> visit = [&](std::size_t i) {
    if (done[i]) return;
    if (stack[i]) return;  // cycle: break at the module that closed it
    stack[i] = 1;
    for (const auto& dep : mappedOrder[i].imports) {
      auto it = index.find(lowerStr(dep));
      if (it == index.end()) continue;  // host/shim/probe: not a mapped module
      visit(it->second);
    }
    stack[i] = 0;
    done[i] = 1;
    out.push_back(mappedOrder[i].name);
  };
  for (std::size_t i = 0; i < mappedOrder.size(); ++i) visit(i);
  return out;
}

std::optional<RunMode> parseRunMode(bool dryRun, bool init, bool run) {
  int n = (dryRun ? 1 : 0) + (init ? 1 : 0) + (run ? 1 : 0);
  if (n != 1) return std::nullopt;
  if (dryRun) return RunMode::DryRun;
  if (init) return RunMode::Init;
  return RunMode::Run;
}

}  // namespace sb
