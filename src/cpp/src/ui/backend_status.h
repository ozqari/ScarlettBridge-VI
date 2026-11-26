#pragma once
// Portable UI support for the Qt control-center shell: version strings plus
// read-only, capped folder scans and one-line backend summaries. No Qt, no OS
// execution, no game content. Independently implemented under MIT; behavior
// mirrors `scarlettbridge-cli validate-folder` so the GUI and the CLI agree.
// Unit-tested in sb_tests without Qt.
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "core/config.h"
#include "core/result.h"
#include "graphics/shader_tally.h"
#include "media/input_script.h"
#include "telemetry/metrics.h"
#include "telemetry/refusal_census.h"

namespace sb {
namespace ui {

struct Versions {
  std::string app;         // "0.1.0"
  std::string build;       // kBuildId
  std::string ipc;         // "1"
  std::string schema;      // "3"
  std::string translator;  // "1"
  std::string cache;       // "1"
  std::string sbm;         // "1"
};

Versions versions();

struct FolderScan {
  bool ok = false;
  std::string error;     // set when !ok
  std::string shownDir;  // "<redacted>" when redacted, else the input dir
  std::size_t entries = 0;
  std::uint64_t bytes = 0;
  std::size_t hashed = 0;
  double ms = 0.0;
};

// Read-only capped scan: max 500 entries, SHA-256 of the first 64 KiB for
// files under 5 MiB (max 40 hashed). Never writes into the folder.
FolderScan scanFolderReadOnly(const std::string& dir, bool redact);

// One-line human summaries for the dashboard/pages. Never throws; an
// unavailable backend is reported as a capability skip, never faked.
std::string systemLine();
std::string gamepadLine();
std::string audioLine();
std::string adapterLine();
std::string cacheFingerprint();

// Read-only shader-cache stats for the Shader Manager page (portable,
// unit-tested; the Qt page is a thin view over this). stats() never writes;
// a missing dir reads as zero entries. Never throws.
struct CacheStats {
  std::size_t entries = 0;
  std::size_t bytes = 0;
  std::size_t maxEntries = 0;
  std::size_t maxBytes = 0;
  bool overBudget = false;
  std::string dir;  // the cache dir as passed in
};
inline constexpr std::size_t kDefaultCacheMaxEntries = 4096;  // ShaderCache default
CacheStats queryCacheStats(const std::string& dir, std::size_t maxEntries,
                           std::size_t maxBytes);
// "2 entries, 12345 bytes (budget 4096 entries / 512 MB) in <dir> — within
// budget" (or "— BUDGET EXHAUSTED, stores refuse"). Never throws.
std::string cacheStatsLine(const CacheStats& s);

// Full schema-v3 option lists + validators for the Graphics Settings page.
// These mirror core/config.cpp allowlists so the GUI and the loader agree;
// invalid values are never silently kept (fromJson reports each correction).
std::vector<std::string> settingBackends();
std::vector<std::string> settingWindowModes();
std::vector<std::string> settingResolutions();
bool isValidBackend(const std::string& v);
bool isValidWindowMode(const std::string& v);
bool isValidResolution(const std::string& v);
// Deterministic one-line summary used by the Graphics page status label.
std::string settingsSummary(const Settings& s);

// Diagnostics/logs export helpers (portable, unit-tested; the Qt pages are
// thin views over these). Exports are always redacted and refuse in-tree
// destinations via ensureOutsideRepo.
std::string redactLine(const std::string& s);
// Case-insensitive substring; empty needle matches everything.
bool matchesFilter(const std::string& haystack, const std::string& needle);
// Versioned JSON: {app, build, ipc, schema, counters:{}, trace:[{t,sys,msg}]}
// with trace messages redacted. Never throws.
std::string diagnosticsJson(const std::vector<std::pair<std::string, double>>& counters,
                            const std::vector<diag::TraceEvent>& trace);
// Writes text to path only when outside every source tree, creating parents.
// Fails (never writes) for empty paths, in-tree paths, or I/O errors.
Result<void> exportTextOutsideRepo(const std::string& path, const std::string& text);

// Refusal-census summaries for the Diagnostics page (portable, unit-tested;
// the Qt page is a thin view over these). Both censuses are real objects
// owned by the shell: zeros mean no translated draw has been refused yet
// (the synthetic ticker binds no descriptors), never a faked backend.
// One-line null-cause summary: total + per-cause counts (sums by
// construction). Never throws.
std::string nullCensusLine(const diag::NullCensus& c);
// One-line per-reason summary: total + distinct keys/pairs + overflow.
// Never throws.
std::string perReasonCensusLine(const diag::PerReasonCensus& census);
// Full report lines (summary + one line per pair) capped to 1 + maxDetail
// detail lines so the Diagnostics view stays bounded. Wraps
// PerReasonCensus::report(/*onlyIfChanged=*/false), so repeated calls do
// not consume delta state. Never throws.
std::vector<std::string> perReasonDetailLines(diag::PerReasonCensus& census,
                                              std::size_t maxDetail);

// Benchmark frame-rate readout for the Hardware Benchmark page (portable,
// unit-tested; the Qt page is a thin view over these). A smoke run is one
// one-shot draw, not a continuous frame stream, so the rate is always
// labelled as a single sample and the bottleneck stays unknown — sustained
// FPS is never derived from a single draw. Never throws.
std::string smokeRateLine(double drawMs);
std::string smokeBottleneckLine();

// Scripted-input summaries for the Controls page (portable, unit-tested;
// the Qt page is a thin view over these plus media/input_script). DEMO only:
// no device, no presenter — waits/marks/groups stay rejections in the
// parsed script. Never throws.
// One-line parse summary: empty note, or entry/rejection counts + base +
// span. Never throws.
std::string scriptSummaryLine(const input::InputScript& script);
// One-line pad state: hex button mask + six axis values. Never throws.
std::string scriptStateLine(const input::ScriptState& state);
// Rejection detail lines ("item <n>: <reason>: <text>", text capped to 80
// glyphs), capped to maxDetail lines so the view stays bounded. Empty when
// the script parsed clean. Never throws.
std::vector<std::string> scriptDetailLines(const input::InputScript& script,
                                            std::size_t maxDetail);

// Engine-instance probe for the Diagnostics page (portable, unit-tested; the
// Qt page is a thin view over this). One-line meaning of a counter instance
// name via diag::parseEngineInstance: "pid=<pid> kind=<kind>", or a
// not-a-name note. There are no live engine figures (OS counter collection
// is not implemented), so this names the rule only. Never throws.
std::string engineInstanceLine(const std::string& name);

// Human-readable folder-scan report for the Setup page (portable,
// unit-tested; the Qt page is a thin view over this plus the guarded
// exportTextOutsideRepo path). The report repeats the scan's shown dir —
// "<redacted>" when the scan redacted — so it is safe to share as the
// finding's record. Never throws.
std::string folderReportText(const FolderScan& scan);

// Shader failure-tally summary for the Shader Manager page (portable,
// unit-tested; the Qt page is a thin view over a shell-owned real
// gfx::FailTally). Zeros mean no batch translator run has recorded a
// failure yet — never a faked backend. Never throws.
// One-line summary: total + distinct reasons.
std::string failTallyLine(const gfx::FailTally& tally);
// Bounded report view: summary line first, then up to maxDetail failure
// report lines (most-frequent-first). Never throws.
std::vector<std::string> failTallyReportLines(const gfx::FailTally& tally,
                                              std::size_t maxDetail);

}  // namespace ui
}  // namespace sb
