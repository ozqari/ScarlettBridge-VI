#include "ipc/protocol.h"

namespace sb {
namespace ipc {
namespace {
bool hasStr(const json::Value& m, const char* k) {
  return m.at(k).type() == json::Value::Type::String;
}
bool hasNum(const json::Value& m, const char* k) {
  return m.at(k).type() == json::Value::Type::Number;
}
}  // namespace

bool isKnownType(const std::string& type) {
  for (const char* t : {"Hello", "HelloAck", "StartRuntime", "StartAccepted", "StopRuntime",
                        "StopAccepted", "RuntimeState", "HardwareInfo", "LogEvent",
                        "ProgressEvent", "ShaderEvent", "FrameStats", "DeviceLost",
                        "UserActionResult"})
    if (type == t) return true;
  return false;
}

Result<void> validate(const json::Value& msg) {
  if (msg.type() != json::Value::Type::Object)
    return Result<void>::fail(Error::make("ipc", 1, "message is not an object"));
  if (static_cast<int>(msg.at("v").asNumber(-1)) != kIpcProtocolVersion)
    return Result<void>::fail(Error::make("ipc", 2, "protocol version mismatch"));
  const std::string type = msg.at("type").asString();
  if (!isKnownType(type))
    return Result<void>::fail(Error::make("ipc", 3, "unknown message type: " + type));
  auto need = [&](std::initializer_list<const char*> strs,
                  std::initializer_list<const char*> nums) -> Result<void> {
    for (const char* k : strs)
      if (!hasStr(msg, k))
        return Result<void>::fail(Error::make("ipc", 4, "missing field '" + std::string(k) + "' in " + type));
    for (const char* k : nums)
      if (!hasNum(msg, k))
        return Result<void>::fail(Error::make("ipc", 5, "missing numeric field '" + std::string(k) + "' in " + type));
    return Result<void>::success();
  };
  if (type == "Hello") return need({"build"}, {});
  if (type == "HelloAck") return need({"build"}, {});
  if (type == "RuntimeState") return need({"state"}, {});
  if (type == "LogEvent") return need({"sev", "sys", "msg"}, {});
  if (type == "ProgressEvent") return need({"op"}, {"done", "total"});
  if (type == "ShaderEvent") return need({"hash", "stage", "status"}, {"ms"});
  if (type == "FrameStats") return need({}, {"cpuMs", "gpuMs"});
  if (type == "DeviceLost") return need({"reason"}, {});
  return Result<void>::success();
}

json::Value hello(const std::string& buildId) {
  json::Value m = json::Value::object();
  m.set("v", json::Value::number(kIpcProtocolVersion));
  m.set("type", json::Value::str("Hello"));
  m.set("build", json::Value::str(buildId));
  return m;
}

json::Value helloAck(const std::string& buildId) {
  json::Value m = hello(buildId);
  m.set("type", json::Value::str("HelloAck"));
  return m;
}

Result<std::vector<std::uint8_t>> encode(const json::Value& msg) {
  if (auto v = validate(msg); !v.ok()) return Result<std::vector<std::uint8_t>>::fail(v.error());
  std::string body = msg.serialize();
  if (body.size() > kIpcMaxMessageBytes)
    return Result<std::vector<std::uint8_t>>::fail(Error::make("ipc", 6, "message exceeds 64 KiB cap"));
  std::vector<std::uint8_t> out(4 + body.size());
  std::uint32_t len = static_cast<std::uint32_t>(body.size());
  out[0] = static_cast<std::uint8_t>(len);
  out[1] = static_cast<std::uint8_t>(len >> 8);
  out[2] = static_cast<std::uint8_t>(len >> 16);
  out[3] = static_cast<std::uint8_t>(len >> 24);
  for (std::size_t i = 0; i < body.size(); ++i) out[4 + i] = static_cast<std::uint8_t>(body[i]);
  return Result<std::vector<std::uint8_t>>::success(std::move(out));
}

Result<json::Value> decode(const std::uint8_t* data, std::size_t len) {
  if (len < 4) return Result<json::Value>::fail(Error::make("ipc", 7, "truncated frame header"));
  std::uint32_t bodyLen = static_cast<std::uint32_t>(data[0]) |
                          (static_cast<std::uint32_t>(data[1]) << 8) |
                          (static_cast<std::uint32_t>(data[2]) << 16) |
                          (static_cast<std::uint32_t>(data[3]) << 24);
  if (bodyLen > kIpcMaxMessageBytes)
    return Result<json::Value>::fail(Error::make("ipc", 8, "frame exceeds 64 KiB cap"));
  if (len - 4 < bodyLen)
    return Result<json::Value>::fail(Error::make("ipc", 9, "truncated frame body"));
  std::string body(reinterpret_cast<const char*>(data + 4), bodyLen);
  Result<json::Value> v = json::Value::parse(body);
  if (!v.ok()) return v;
  if (auto ok = validate(v.value()); !ok.ok()) return Result<json::Value>::fail(ok.error());
  return v;
}

}  // namespace ipc
}  // namespace sb
