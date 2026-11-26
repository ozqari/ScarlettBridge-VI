// sb_cache_inspector: stats / verify / clear a shader-cache directory.
// Clear requires --confirm and only touches *.bin in the given directory.
#include <iostream>
#include <string>
#include "graphics/pipeline_cache.h"

int main(int argc, char** argv) {
  std::string dir;
  std::string cmd = "stats";
  bool confirm = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--dir" && i + 1 < argc) dir = argv[++i];
    if (a == "--confirm") confirm = true;
    if (a == "stats" || a == "clear" || a == "verify") cmd = a;
  }
  if (dir.empty()) {
    std::cerr << "usage: sb_cache_inspector (stats|clear|verify) --dir <path> [--confirm]\n";
    return 2;
  }
  sb::cache::ShaderCache cache(dir, 4096, 512u * 1024u * 1024u);
  if (cmd == "stats" || cmd == "verify") {
    std::size_t entries = 0, bytes = 0;
    cache.stats(entries, bytes);
    std::cout << "{\"dir\":\"" << dir << "\",\"entries\":" << entries << ",\"bytes\":" << bytes
              << "}\n";
    return 0;
  }
  if (!confirm) {
    std::cerr << "refusing to clear without --confirm (prints path + size first)\n";
    std::size_t entries = 0, bytes = 0;
    cache.stats(entries, bytes);
    std::cerr << "would clear " << entries << " entries, " << bytes << " bytes in " << dir << "\n";
    return 2;
  }
  auto r = cache.clear();
  std::cout << (r.ok() ? "{\"cleared\":true}\n" : "{\"cleared\":false}\n");
  return r.ok() ? 0 : 1;
}
