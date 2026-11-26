#include "core/config.h"
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>

namespace sb {
namespace {
bool oneOf(const std::string& v, std::initializer_list<const char*> opts) {
  for (const char* o : opts)
    if (v == o) return true;
  return false;
}
int clampInt(const json::Value& v, int lo, int hi, int fallback, const char* name,
             std::vector<std::string>& warn) {
  if (v.type() != json::Value::Type::Number) {
    warn.push_back(std::string("config: '") + name + "' not a number, using default");
    return fallback;
  }
  int i = static_cast<int>(v.asNumber());
  if (i < lo || i > hi) {
    warn.push_back(std::string("config: '") + name + "' out of range, clamped");
    return i < lo ? lo : hi;
  }
  return i;
}
}  // namespace

Settings defaults() {
  Settings s;
  s.cachePath = "%LOCALAPPDATA%/scarlettbridge/shader-cache";
  return s;
}

SettingsLoad fromJson(const json::Value& v) {
  SettingsLoad out;
  Settings d = defaults();
  if (v.type() != json::Value::Type::Object) {
    out.warnings.push_back("config: root is not an object, using defaults");
    out.settings = d;
    return out;
  }
  int schema = static_cast<int>(v.at("schema").asNumber(0));
  if (schema != 3)
    out.warnings.push_back("config: schema migrated to v3");
  Settings s = d;
  s.schema = 3;
  auto str = [&](const char* k, std::string cur, std::initializer_list<const char*> opts) {
    const json::Value& f = v.at(k);
    if (f.type() != json::Value::Type::String) {
      if (f.type() != json::Value::Type::Null)
        out.warnings.push_back(std::string("config: '") + k + "' not a string, using default");
      return cur;
    }
    if (!oneOf(f.asString(), opts)) {
      out.warnings.push_back(std::string("config: '") + k + "' has unsupported value '" +
                             f.asString() + "', using default");
      return cur;
    }
    return f.asString();
  };
  s.adapter = v.at("adapter").type() == json::Value::Type::String ? v.at("adapter").asString() : d.adapter;
  s.backend = str("backend", d.backend, {"d3d12"});
  s.windowMode = str("windowMode", d.windowMode, {"windowed", "borderless", "fullscreen"});
  s.resolution = str("resolution", d.resolution, {"1280x720", "1920x1080", "2560x1440", "3840x2160"});
  auto boolean = [&](const char* k, bool cur) {
    const json::Value& f = v.at(k);
    if (f.type() != json::Value::Type::Bool) {
      if (f.type() != json::Value::Type::Null)
        out.warnings.push_back(std::string("config: '") + k + "' not a bool, using default");
      return cur;
    }
    return f.asBool();
  };
  s.vsync = boolean("vsync", d.vsync);
  s.frameCap = clampInt(v.at("frameCap"), 0, 1000, d.frameCap, "frameCap", out.warnings);
  s.texBudgetMB = clampInt(v.at("texBudgetMB"), 256, 32768, d.texBudgetMB, "texBudgetMB", out.warnings);
  s.shaderCache = boolean("shaderCache", d.shaderCache);
  s.cacheMB = clampInt(v.at("cacheMB"), 16, 32768, d.cacheMB, "cacheMB", out.warnings);
  if (v.at("cachePath").type() == json::Value::Type::String)
    s.cachePath = v.at("cachePath").asString();
  s.precompile = boolean("precompile", d.precompile);
  s.debugLayer = boolean("debugLayer", d.debugLayer);
  s.dred = boolean("dred", d.dred);
  // Vulkan/WebGPU stay disabled until a backend passes conformance (brief 7.3).
  out.settings = s;
  return out;
}

json::Value toJson(const Settings& s) {
  json::Value v = json::Value::object();
  v.set("schema", json::Value::number(s.schema));
  v.set("adapter", json::Value::str(s.adapter));
  v.set("backend", json::Value::str(s.backend));
  v.set("windowMode", json::Value::str(s.windowMode));
  v.set("resolution", json::Value::str(s.resolution));
  v.set("vsync", json::Value::boolean(s.vsync));
  v.set("frameCap", json::Value::number(s.frameCap));
  v.set("texBudgetMB", json::Value::number(s.texBudgetMB));
  v.set("shaderCache", json::Value::boolean(s.shaderCache));
  v.set("cacheMB", json::Value::number(s.cacheMB));
  v.set("cachePath", json::Value::str(s.cachePath));
  v.set("precompile", json::Value::boolean(s.precompile));
  v.set("debugLayer", json::Value::boolean(s.debugLayer));
  v.set("dred", json::Value::boolean(s.dred));
  return v;
}

Result<SettingsLoad> loadFile(const std::string& path) {
  std::ifstream f(std::filesystem::path(path), std::ios::binary);
  if (!f) {
    // Missing file is not an error: first-run defaults (reported by caller).
    SettingsLoad out;
    out.settings = defaults();
    out.warnings.push_back("config: file not found, using defaults");
    return Result<SettingsLoad>::success(std::move(out));
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  Result<json::Value> v = json::Value::parse(ss.str());
  if (!v.ok()) return Result<SettingsLoad>::fail(v.error());
  return Result<SettingsLoad>::success(fromJson(v.value()));
}

Result<void> saveFile(const std::string& path, const Settings& s) {
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
  if (ec) return Result<void>::fail(Error::make("config", 1, "cannot create config dir: " + ec.message()));
  std::ofstream f(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
  if (!f) return Result<void>::fail(Error::make("config", 2, "cannot open config file for write"));
  f << toJson(s).serialize();
  if (!f) return Result<void>::fail(Error::make("config", 3, "config write failed"));
  return Result<void>::success();
}

}  // namespace sb
