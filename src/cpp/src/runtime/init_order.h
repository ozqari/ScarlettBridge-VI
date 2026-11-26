#pragma once
// Module initialisation order + binding report + run-mode guard.
// Pure logic over module names; no mapping, no execution.
//
// Idea informed by ReCity runtime/src/pe_loader.h + main.cpp
// (Loader::initialisation_order: each module after every module it imports,
// cycles broken at the closer; mapping order puts an importer before its
// imports; binding counts shim/host/module/unimplemented per module printed
// before running; exactly-one-mode --dry-run/--init/--run guard) and XCL
// modules::register_mapped/find_mapped_by_name idempotence. Independently
// implemented here in portable C++; no code copied.
#include <optional>
#include <string>
#include <vector>
#include "runtime/resolve.h"  // ResolvedSource for binding counts

namespace sb {

enum class Binding { Shim, Host, Module, Unimplemented };

std::string toString(Binding b);

// HLE/HleOther are our implementations (shims); System is the real host;
// GameDll is a bundled title module; Stub has no provider.
Binding bindingFromSource(ResolvedSource s);

struct BindingCounts {
  int shim = 0;
  int host = 0;
  int module = 0;
  int unimplemented = 0;
  int total() const { return shim + host + module + unimplemented; }
};

BindingCounts countBindings(const std::vector<ResolvedSource>& sources);

struct ModuleNode {
  std::string name;                 // file name as mapped ("game.exe")
  std::vector<std::string> imports;  // DLL names this module imports
};

// Dependencies-first order over mapping-order nodes: each module runs after
// every module it imports. Imports that name no mapped module are ignored
// (host/shim/probe). Cycles are broken at the module that closes them, so
// the output always contains every input module exactly once. Matching is
// case-insensitive on file names.
std::vector<std::string> computeInitOrder(const std::vector<ModuleNode>& mappedOrder);

enum class RunMode { DryRun, Init, Run };

// Exactly one of the three flags must be set (ReCity's --dry-run/--init/--run
// guard: dry-run with run would otherwise initialise modules and call the
// entry point). Returns nullopt unless exactly one is true.
std::optional<RunMode> parseRunMode(bool dryRun, bool init, bool run);

}  // namespace sb
