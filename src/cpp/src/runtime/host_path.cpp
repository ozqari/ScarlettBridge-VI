#include "runtime/host_path.h"

#include <cctype>

namespace sb {
namespace {

std::string lowerStr(const std::string& s) {
  std::string out = s;
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

}  // namespace

std::string fileNameOfPath(const std::string& path) {
  std::size_t cut = path.find_last_of("/\\");
  return cut == std::string::npos ? path : path.substr(cut + 1);
}

std::string system32PathFor(const std::string& dllFileName) {
  return std::string("C:\\Windows\\System32\\") + fileNameOfPath(dllFileName);
}

bool isSystem32Path(const std::string& path) {
  std::string p = lowerStr(path);
  for (char& c : p)
    if (c == '/') c = '\\';
  // Match "<drive>:\windows\system32\<file>" with a non-empty file part.
  if (p.size() < 22) return false;  // e.g. "c:\windows\system32\x" is 22
  if (p[1] != ':' || p[2] != '\\') return false;
  const std::string prefix = "\\windows\\system32\\";
  if (p.compare(1, prefix.size(), prefix) != 0) return false;
  return p.size() > 2 + prefix.size();
}

bool isPlantingAttempt(const std::string& resolvedPath, const std::string& expectedDll) {
  if (lowerStr(fileNameOfPath(resolvedPath)) != lowerStr(fileNameOfPath(expectedDll)))
    return false;
  return !isSystem32Path(resolvedPath);
}

}  // namespace sb
