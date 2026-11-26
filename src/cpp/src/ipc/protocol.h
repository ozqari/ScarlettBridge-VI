#pragma once
// Versioned local protocol v1. Same message families and versioning rules as
// the web prototype's SB_IPC: handshake-checked version, rejected unknown
// required fields/invalid enums, 64 KiB message cap, 512 queue cap.
// Framing: u32 LE length prefix + UTF-8 JSON. No network transport exists.
#include <string>
#include <vector>
#include "core/json.h"
#include "core/result.h"
#include "core/version.h"

namespace sb {
namespace ipc {

inline const char* kPipeNamePrefix = "\\\\.\\pipe\\scarlettbridge-v1-";

bool isKnownType(const std::string& type);
// Required-field validation per message type. Unknown required fields and
// invalid enum values are rejected; unknown OPTIONAL fields are ignored.
Result<void> validate(const json::Value& msg);
json::Value hello(const std::string& buildId);
json::Value helloAck(const std::string& buildId);
// Framing helpers with size caps.
Result<std::vector<std::uint8_t>> encode(const json::Value& msg);
Result<json::Value> decode(const std::uint8_t* data, std::size_t len);

}  // namespace ipc
}  // namespace sb
