#pragma once
// Host DLL paths must come from System32 only — never from the title folder
// or the current working directory.
//
// Idea informed by victory-fork crates/victory-hle/tests/no_dll_planting.rs
// (an allow-listed name copied next to the title must NOT satisfy the host)
// and AnyXboxX xgameruntime.cpp (SystemRuntime loads by full System32 path
// with LOAD_LIBRARY_SEARCH_SYSTEM32). Independently implemented here as
// portable path helpers; the Windows loader call itself stays in the
// platform backend. No code copied.
#include <string>

namespace sb {

std::string fileNameOfPath(const std::string& path);
// "C:\\Windows\\System32\\<file>" for a DLL file name. Any directory part
// of the input is dropped, so "evil\\kernel32.dll" cannot redirect the load.
std::string system32PathFor(const std::string& dllFileName);
// True when `path` is directly inside the Windows System32 directory
// (either slash style, any drive-letter case).
bool isSystem32Path(const std::string& path);
// True when a resolved host path is a planting attempt: same file name as
// expected but not in System32.
bool isPlantingAttempt(const std::string& resolvedPath, const std::string& expectedDll);

}  // namespace sb
