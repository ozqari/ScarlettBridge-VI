#include "runtime/title.h"

#include <cctype>
#include <filesystem>

namespace sb {
namespace {

std::string trimStr(const std::string& s) {
  std::size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

std::string decodeEntities(std::string s) {
  const char* pairs[][2] = {{"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
                            {"&apos;", "'"}, {"&amp;", "&"}};
  for (auto& p : pairs) {
    std::string out;
    std::size_t at = 0;
    while (true) {
      std::size_t f = s.find(p[0], at);
      if (f == std::string::npos) {
        out += s.substr(at);
        break;
      }
      out += s.substr(at, f - at);
      out += p[1];
      at = f + std::string(p[0]).size();
    }
    s = std::move(out);
  }
  return s;
}

bool eqFold(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  return true;
}

std::string fileNameOf(const std::string& p) {
  std::size_t s = p.find_last_of("/\\");
  return s == std::string::npos ? p : p.substr(s + 1);
}

}  // namespace

std::string TitleManifest::savesDir() const {
  if (storageDir.empty()) return "saves";
  std::filesystem::path p(storageDir);
  p.replace_filename("saves");
  return p.string();
}

TitleManifest TitleManifest::parse(const std::string& xmlText) {
  // Element scan: path stack from the root (Game/TitleId), trimmed text up to
  // the first child, entities decoded; comments, CDATA, declarations skipped.
  std::vector<std::pair<std::string, std::string>> elements;
  std::vector<std::string> path;
  std::string rest = xmlText;
  // Strip a UTF-8 BOM if present.
  if (rest.size() >= 3 && static_cast<unsigned char>(rest[0]) == 0xEF &&
      static_cast<unsigned char>(rest[1]) == 0xBB && static_cast<unsigned char>(rest[2]) == 0xBF)
    rest = rest.substr(3);
  while (true) {
    std::size_t open = rest.find('<');
    if (open == std::string::npos) break;
    rest = rest.substr(open);
    std::size_t end = std::string::npos;
    if (rest.rfind("<!--", 0) == 0)
      end = rest.find("-->");
    else if (rest.rfind("<![CDATA[", 0) == 0)
      end = rest.find("]]>");
    else
      end = rest.find('>');
    if (end == std::string::npos) break;
    bool cdata = rest.rfind("<![CDATA[", 0) == 0;
    bool comment = rest.rfind("<!--", 0) == 0;
    std::string tag = rest.substr(1, end - 1);
    rest = rest.substr(end + (cdata ? 3 : comment ? 3 : 1));
    if (comment || cdata) continue;
    if (tag.empty()) continue;
    if (tag[0] == '!' || tag[0] == '?') continue;
    if (tag[0] == '/') {
      std::string name = trimStr(tag.substr(1));
      for (std::size_t i = path.size(); i-- > 0;) {
        if (path[i] == name) {
          path.resize(i);
          break;
        }
      }
      continue;
    }
    bool closed = !tag.empty() && tag.back() == '/';
    std::string name = trimStr(tag);
    std::size_t sp = name.find_first_of(" \t\r\n/");
    if (sp != std::string::npos) name = name.substr(0, sp);
    if (name.empty()) continue;
    path.push_back(name);
    std::size_t lt = rest.find('<');
    std::string body = trimStr(lt == std::string::npos ? rest : rest.substr(0, lt));
    std::string joined;
    for (std::size_t i = 0; i < path.size(); ++i) {
      if (i) joined += '/';
      joined += path[i];
    }
    elements.emplace_back(joined, decodeEntities(body));
    if (closed) path.pop_back();
  }

  TitleManifest m;
  for (const auto& e : elements) {
    if (e.first == "Game/TitleId") {
      try {
        m.titleId = static_cast<std::uint32_t>(std::stoul(trimStr(e.second), nullptr, 16));
      } catch (...) {
      }
    } else if (e.first == "Game/StoreId") {
      m.storeId = e.second;
    } else if (e.first == "Game/PersistentLocalStorage/SizeMB") {
      try {
        unsigned long mb = std::stoul(trimStr(e.second));
        m.storageBytes = static_cast<std::uint64_t>(mb) << 20;
      } catch (...) {
      }
    }
  }
  return m;
}

void ImageRegistry::setImagePath(std::uint64_t base, const std::string& path) {
  std::string abs = path;
  try {
    abs = std::filesystem::absolute(std::filesystem::path(path)).string();
  } catch (...) {
  }
  std::lock_guard<std::mutex> l(mu_);
  for (auto it = paths_.begin(); it != paths_.end();) {
    if (it->first == base)
      it = paths_.erase(it);
    else
      ++it;
  }
  paths_[base] = abs;
}

void ImageRegistry::forgetImage(std::uint64_t base) {
  std::lock_guard<std::mutex> l(mu_);
  paths_.erase(base);
}

std::string ImageRegistry::imagePath(std::uint64_t base) const {
  std::lock_guard<std::mutex> l(mu_);
  auto it = paths_.find(base);
  return it == paths_.end() ? "" : it->second;
}

std::uint64_t ImageRegistry::imageNamed(const std::string& fileName) const {
  std::lock_guard<std::mutex> l(mu_);
  for (const auto& kv : paths_)
    if (eqFold(fileNameOf(kv.second), fileName)) return kv.first;
  return 0;
}

}  // namespace sb
