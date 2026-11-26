#pragma once
// Fixed-width frame-rate/load summary ("frame box") for diagnostics.
//
// Idea informed by victory-fork `crates/victory-gpu/src/fpsbox.rs`: its
// six-line box (rate + ms, CPU/game cores, top thread, GPU/game share, VRAM
// used/budget, RAM used/free), tenths formatting, cut-or-pad `fit`, the
// Off/Rate/All mode switch, and the module comment's bottleneck rule — a
// thread near 100 % beside an idle card means the thread holds the frame, a
// card near 100 % holds it itself, neither busy means waiting. Independently
// implemented here in portable C++ with our own types and wording; no OS
// calls, no GPU work, no game content. Upstream's CPU glyph bitmap, pixel
// packing, four-buffer present ring, and OS stat collection are NOT
// implemented (GPU/OS-coupled). Unit-tested in sb_tests.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sb {
namespace diag {

// What the summary shows: nothing, the rate alone, or the whole box.
enum class FrameBoxMode { Off, Rate, All };

// "0" -> Off, "fps" -> Rate, anything else (including empty) -> All. Pure;
// reading the environment variable is the app's job. Never throws.
FrameBoxMode parseFrameBoxMode(const std::string& v);

// Tenths of a frame a second from a measured rate: nullopt when the rate is
// unusable (negative, NaN, or infinite); otherwise round(fps*10) clamped to
// [0, 99999]. Never throws.
std::optional<unsigned> tenthsFromFps(double fps);

// "10.3 FPS" for tenths=103 (clamped to 99999). Never throws.
std::string frameRateText(unsigned tenths);
// "5.8 FPS  172 MS" (ms = 10000/tenths, rounded); tenths==0 gives the rate
// alone. Never throws.
std::string frameRateMsText(unsigned tenths);

// `line` in exactly `columns` glyphs: cut, or space-filled. Our output is
// ASCII-only, so bytes are glyphs. Never throws.
std::string fitBoxLine(const std::string& line, std::size_t columns);

// Optional machine-load look + card memory for the full box. Plain data.
struct FrameBoxStats {
  double cpuAll = 0.0;     // percent, all processors, all programs
  double cores = 0.0;      // this process, in cores-equivalent
  double topThread = 0.0;  // this process's busiest thread, of one processor
  bool gpuKnown = false;
  double gpuAll = 0.0;   // the card's busiest engine, all programs
  double gpuOurs = 0.0;  // this process's share
  double ramOursMB = 0.0;
  double ramFreeMB = 0.0;
};

struct FrameBoxCard {
  std::uint64_t budgetBytes = 0;
  std::uint64_t usedBytes = 0;
};

// The six full-box lines, each exactly kFrameBoxColumns glyphs: rate, CPU,
// top thread, GPU, VRAM, RAM. A missing rate gives a blank first line;
// missing stats or card blank their lines. Never throws.
inline constexpr std::size_t kFrameBoxColumns = 26;
std::vector<std::string> frameBoxLines(std::optional<unsigned> tenths,
                                       std::optional<FrameBoxStats> stats,
                                       std::optional<FrameBoxCard> card);

// Which side holds the frame, from the bottleneck rule above: a saturated
// card decides first, then a saturated thread; with no card figures and no
// hot thread there is nothing to say. "Near 100 %" means >= 90 here.
enum class Bottleneck { Unknown, ThreadHeld, GpuHeld, Waiting };
Bottleneck classifyBottleneck(const FrameBoxStats& s);
const char* bottleneckName(Bottleneck b);

}  // namespace diag
}  // namespace sb
