#pragma once
// Host system report: CPU model, logical processors, RAM, OS build.
// No game data touched. Used by the dashboard, logs, and benchmark exports.
#include <cstdint>
#include <string>
#include "core/result.h"

namespace sb {
namespace sysinfo {

struct SystemReport {
  std::string cpuModel;
  unsigned logicalProcessors = 0;
  std::uint64_t ramMB = 0;
  std::string osVersion;    // e.g. "Windows 11 build 22631"
  std::string windowsBuild;  // bare build number, "" when unknown
};

Result<SystemReport> collect();

}  // namespace sysinfo
}  // namespace sb
