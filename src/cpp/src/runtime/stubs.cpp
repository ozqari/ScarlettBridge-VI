#include "runtime/stubs.h"

#include <cctype>

namespace sb {

std::string StubDatabase::lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string StubDatabase::key(const std::string& dll, const std::string& name) {
  return lower(dll) + "!" + lower(name);
}

void StubDatabase::setModuleDefault(const std::string& dll, std::uint64_t ret) {
  std::lock_guard<std::mutex> lock(mu_);
  moduleDefaults_[lower(dll)] = ret;
}

void StubDatabase::setFunctionReturn(const std::string& dll, const std::string& name,
                                     std::uint64_t ret) {
  std::lock_guard<std::mutex> lock(mu_);
  functions_[key(dll, name)] = ret;
}

void StubDatabase::setOrdinalName(const std::string& dll, std::uint16_t ordinal,
                                  const std::string& name) {
  std::lock_guard<std::mutex> lock(mu_);
  ordinals_[lower(dll)][ordinal] = name;
}

std::uint64_t StubDatabase::returnFor(const std::string& dll, const std::string& name,
                                      const std::string& fallbackDll) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto findFn = [&](const std::string& d) -> const std::uint64_t* {
    auto it = functions_.find(key(d, name));
    return it == functions_.end() ? nullptr : &it->second;
  };
  if (const auto* r = findFn(dll)) return *r;
  auto wit = functions_.find(key("*", name));
  if (wit != functions_.end()) return wit->second;
  auto mit = moduleDefaults_.find(lower(dll));
  if (mit != moduleDefaults_.end()) return mit->second;
  if (!fallbackDll.empty() && lower(fallbackDll) != lower(dll)) {
    if (const auto* r = findFn(fallbackDll)) return *r;
    auto fit = moduleDefaults_.find(lower(fallbackDll));
    if (fit != moduleDefaults_.end()) return fit->second;
  }
  return 0;
}

std::string StubDatabase::ordinalName(const std::string& dll,
                                      std::uint16_t ordinal) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto dit = ordinals_.find(lower(dll));
  if (dit == ordinals_.end()) return "";
  auto oit = dit->second.find(ordinal);
  return oit == dit->second.end() ? "" : oit->second;
}

std::uint64_t StubDatabase::recordCall(const std::string& dll, const std::string& name,
                                       std::uint64_t caller) {
  std::lock_guard<std::mutex> lock(mu_);
  std::string k = key(dll, name);
  auto it = calls_.find(k);
  if (it == calls_.end()) {
    StubCallInfo info;
    info.dll = dll;
    info.name = name;
    auto fit = functions_.find(k);
    info.configuredRet = (fit != functions_.end()) ? fit->second : 0;
    auto mit = moduleDefaults_.find(lower(dll));
    if (fit == functions_.end() && mit != moduleDefaults_.end())
      info.configuredRet = mit->second;
    info.calls = 1;
    info.firstCaller = caller;
    calls_[k] = info;
    return info.configuredRet;
  }
  it->second.calls += 1;
  return it->second.configuredRet;
}

std::vector<StubCallInfo> StubDatabase::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<StubCallInfo> out;
  out.reserve(calls_.size());
  for (const auto& kv : calls_) out.push_back(kv.second);
  return out;
}

}  // namespace sb
