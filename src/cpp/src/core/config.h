#pragma once
// Settings schema v3: parse, validate, migrate, persist. Mirrors the web
// prototype defaults. Unknown fields are ignored; invalid values are
// replaced with defaults and reported (never silently kept).
#include <string>
#include <vector>
#include "core/json.h"
#include "core/result.h"

namespace sb {

struct Settings {
  int schema = 3;
  std::string adapter = "auto";      // "auto" | adapter LUID string
  std::string backend = "d3d12";     // "d3d12" only; others disabled until tested
  std::string windowMode = "windowed";
  std::string resolution = "1280x720";
  bool vsync = true;
  int frameCap = 60;
  int texBudgetMB = 2048;
  bool shaderCache = true;
  int cacheMB = 512;
  std::string cachePath;
  bool precompile = false;
  bool debugLayer = false;
  bool dred = false;
};

struct SettingsLoad {
  Settings settings;
  std::vector<std::string> warnings;  // every correction applied
};

Settings defaults();
SettingsLoad fromJson(const json::Value& v);  // migrates schema < 3
json::Value toJson(const Settings& s);
Result<SettingsLoad> loadFile(const std::string& path);
Result<void> saveFile(const std::string& path, const Settings& s);

}  // namespace sb
