#pragma once
// Single place for every protocol/schema/build version constant.
#include <cstdint>

namespace sb {
inline constexpr const char* kAppVersion = "0.1.0";
inline constexpr const char* kBuildId = "cpp-skeleton";
inline constexpr int kIpcProtocolVersion = 1;
inline constexpr int kConfigSchemaVersion = 3;
inline constexpr int kTranslatorVersion = 1;
inline constexpr int kPipelineCacheVersion = 1;
inline constexpr int kSbmFormatVersion = 1;
inline constexpr std::uint32_t kSbmMagic = 0x314D4253u;  // "SBM1" LE
inline constexpr std::size_t kIpcMaxMessageBytes = 64u * 1024u;
inline constexpr std::size_t kIpcMaxQueueLength = 512u;
}  // namespace sb
