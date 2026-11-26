#include "telemetry/frame_box.h"

#include <cmath>
#include <cstdio>

namespace sb {
namespace diag {

FrameBoxMode parseFrameBoxMode(const std::string& v) {
  if (v == "0") return FrameBoxMode::Off;
  if (v == "fps") return FrameBoxMode::Rate;
  return FrameBoxMode::All;
}

std::optional<unsigned> tenthsFromFps(double fps) {
  if (!(fps >= 0.0) || !std::isfinite(fps)) return std::nullopt;
  double t = std::round(fps * 10.0);
  if (t > 99999.0) t = 99999.0;
  return static_cast<unsigned>(t);
}

std::string frameRateText(unsigned tenths) {
  if (tenths > 99999u) tenths = 99999u;
  char b[32];
  std::snprintf(b, sizeof(b), "%u.%u FPS", tenths / 10u, tenths % 10u);
  return std::string(b);
}

std::string frameRateMsText(unsigned tenths) {
  std::string r = frameRateText(tenths);
  if (tenths == 0) return r;
  char b[32];
  std::snprintf(b, sizeof(b), "%.0f", 10000.0 / static_cast<double>(tenths));
  r += "  ";
  r += b;
  r += " MS";
  return r;
}

std::string fitBoxLine(const std::string& line, std::size_t columns) {
  if (line.size() >= columns) return line.substr(0, columns);
  return line + std::string(columns - line.size(), ' ');
}

namespace {
// One-decimal gigabytes from a byte count (via MiB, like the box's MB math).
std::string gbFromBytes(std::uint64_t bytes) {
  char b[32];
  std::snprintf(b, sizeof(b), "%.1f",
                static_cast<double>(bytes >> 20) / 1024.0);
  return std::string(b);
}
std::string gbFromMB(double mb) {
  char b[32];
  std::snprintf(b, sizeof(b), "%.1f", mb / 1024.0);
  return std::string(b);
}
}  // namespace

std::vector<std::string> frameBoxLines(std::optional<unsigned> tenths,
                                       std::optional<FrameBoxStats> stats,
                                       std::optional<FrameBoxCard> card) {
  std::vector<std::string> out;
  out.reserve(6);
  out.push_back(tenths ? frameRateMsText(*tenths) : std::string());
  if (stats) {
    const FrameBoxStats& s = *stats;
    char b[64];
    std::snprintf(b, sizeof(b), "CPU %.0f%%  GAME %.1f CORES", s.cpuAll, s.cores);
    out.push_back(b);
    std::snprintf(b, sizeof(b), "TOP THREAD %.0f%%", s.topThread);
    out.push_back(b);
    if (s.gpuKnown) {
      std::snprintf(b, sizeof(b), "GPU %.0f%%  GAME %.0f%%", s.gpuAll, s.gpuOurs);
      out.push_back(b);
    } else {
      out.push_back("GPU NOT KNOWN");
    }
  } else {
    out.push_back(std::string());
    out.push_back(std::string());
    out.push_back(std::string());
  }
  if (card) {
    out.push_back("VRAM " + gbFromBytes(card->usedBytes) + "/" +
                  gbFromBytes(card->budgetBytes) + " GB");
  } else {
    out.push_back(std::string());
  }
  if (stats) {
    out.push_back("RAM " + gbFromMB(stats->ramOursMB) + " GB  FREE " +
                  gbFromMB(stats->ramFreeMB) + " GB");
  } else {
    out.push_back(std::string());
  }
  for (auto& line : out) line = fitBoxLine(line, kFrameBoxColumns);
  return out;
}

Bottleneck classifyBottleneck(const FrameBoxStats& s) {
  if (s.gpuKnown && s.gpuAll >= 90.0) return Bottleneck::GpuHeld;
  if (s.topThread >= 90.0) return Bottleneck::ThreadHeld;
  if (!s.gpuKnown) return Bottleneck::Unknown;
  return Bottleneck::Waiting;
}

const char* bottleneckName(Bottleneck b) {
  switch (b) {
    case Bottleneck::ThreadHeld: return "thread-held";
    case Bottleneck::GpuHeld: return "gpu-held";
    case Bottleneck::Waiting: return "waiting";
    case Bottleneck::Unknown: return "unknown";
  }
  return "?";
}

}  // namespace diag
}  // namespace sb
