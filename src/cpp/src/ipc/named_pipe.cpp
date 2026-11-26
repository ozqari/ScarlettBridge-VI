#include "ipc/named_pipe.h"
#include "core/version.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <sddl.h>
#include <windows.h>

namespace sb {
namespace ipc {

PipeHandle PipeServer::invalidHandle() { return nullptr; }

namespace {
Result<std::vector<std::uint8_t>> readExact(HANDLE h, std::size_t n) {
  std::vector<std::uint8_t> buf(n);
  std::size_t done = 0;
  while (done < n) {
    DWORD got = 0;
    if (!ReadFile(h, buf.data() + done, static_cast<DWORD>(n - done), &got, nullptr) ||
        got == 0)
      return Result<std::vector<std::uint8_t>>::fail(
          Error::make("ipc", 20, "pipe read failed, error " + std::to_string(GetLastError())));
    done += got;
  }
  return Result<std::vector<std::uint8_t>>::ok(std::move(buf));
}
Result<void> writeExact(HANDLE h, const std::uint8_t* p, std::size_t n) {
  std::size_t done = 0;
  while (done < n) {
    DWORD put = 0;
    if (!WriteFile(h, p + done, static_cast<DWORD>(n - done), &put, nullptr))
      return Result<void>::fail(
          Error::make("ipc", 21, "pipe write failed, error " + std::to_string(GetLastError())));
    done += put;
  }
  return Result<void>::ok();
}
}  // namespace

Result<void> PipeServer::create(const std::string& pipeName) {
  if (handle_) return Result<void>::fail(Error::make("ipc", 22, "pipe already created"));
  // Fail-closed ACL: owner (current user) only. No permissive fallback.
  PSECURITY_DESCRIPTOR sd = nullptr;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
          "D:(A;;GA;;;OW)", SDDL_REVISION_1, &sd, nullptr))
    return Result<void>::fail(Error::make("ipc", 23, "cannot build pipe security descriptor"));
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = sd;
  HANDLE h = CreateNamedPipeA(pipeName.c_str(), PIPE_ACCESS_DUPLEX,
                              PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                              1, 65536, 65536, 0, &sa);
  LocalFree(sd);
  if (h == INVALID_HANDLE_VALUE)
    return Result<void>::fail(Error::make(
        "ipc", 24, "CreateNamedPipe failed, error " + std::to_string(GetLastError())));
  handle_ = h;
  return Result<void>::ok();
}

Result<void> PipeServer::accept() {
  if (!handle_) return Result<void>::fail(Error::make("ipc", 25, "pipe not created"));
  if (!ConnectNamedPipe(handle_, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED)
    return Result<void>::fail(Error::make(
        "ipc", 26, "client accept failed, error " + std::to_string(GetLastError())));
  return Result<void>::ok();
}

Result<bool> PipeServer::acceptWithTimeout(unsigned ms) {
  if (!handle_) return Result<bool>::fail(Error::make("ipc", 25, "pipe not created"));
  OVERLAPPED ov{};
  ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  if (!ov.hEvent)
    return Result<bool>::fail(Error::make("ipc", 36, "accept event failed"));
  BOOL ok = ConnectNamedPipe(handle_, &ov);
  bool connected = false;
  Result<bool> out = Result<bool>::ok(false);
  if (ok) {
    connected = true;  // completed synchronously (rare)
  } else {
    DWORD e = GetLastError();
    if (e == ERROR_IO_PENDING) {
      DWORD w = WaitForSingleObject(ov.hEvent, ms);
      if (w == WAIT_OBJECT_0) {
        DWORD dummy = 0;
        connected = !!GetOverlappedResult(handle_, &ov, &dummy, FALSE);
        if (!connected) DisconnectNamedPipe(handle_);
      } else if (w == WAIT_TIMEOUT) {
        CancelIo(handle_);  // client may retry; pipe returns to listening state
        DisconnectNamedPipe(handle_);
      } else {
        CancelIo(handle_);
        DisconnectNamedPipe(handle_);
        out = Result<bool>::fail(Error::make("ipc", 37, "accept wait failed"));
      }
    } else if (e == ERROR_PIPE_CONNECTED) {
      connected = true;
    } else {
      out = Result<bool>::fail(Error::make(
          "ipc", 26, "client accept failed, error " + std::to_string(e)));
    }
  }
  CloseHandle(ov.hEvent);
  if (out.ok() && connected) return Result<bool>::ok(true);
  return out;
}

Result<void> PipeServer::writeFrame(const std::vector<std::uint8_t>& frame) {
  if (!handle_) return Result<void>::fail(Error::make("ipc", 27, "pipe not connected"));
  if (frame.size() > kIpcMaxMessageBytes + 4)
    return Result<void>::fail(Error::make("ipc", 28, "frame over cap"));
  return writeExact(handle_, frame.data(), frame.size());
}

Result<std::vector<std::uint8_t>> PipeServer::readFrame() {
  if (!handle_)
    return Result<std::vector<std::uint8_t>>::fail(Error::make("ipc", 29, "pipe not connected"));
  auto hdr = readExact(handle_, 4);
  if (!hdr.ok()) return hdr;
  const auto* d = hdr.value().data();
  std::uint32_t bodyLen =
      static_cast<std::uint32_t>(d[0]) | (static_cast<std::uint32_t>(d[1]) << 8) |
      (static_cast<std::uint32_t>(d[2]) << 16) | (static_cast<std::uint32_t>(d[3]) << 24);
  if (bodyLen > kIpcMaxMessageBytes)
    return Result<std::vector<std::uint8_t>>::fail(Error::make("ipc", 30, "incoming frame over cap"));
  auto body = readExact(handle_, bodyLen);
  if (!body.ok()) return body;
  std::vector<std::uint8_t> frame(4 + bodyLen);
  for (int i = 0; i < 4; ++i) frame[i] = d[i];
  for (std::size_t i = 0; i < bodyLen; ++i) frame[4 + i] = body.value()[i];
  return Result<std::vector<std::uint8_t>>::ok(std::move(frame));
}

void PipeServer::close() {
  if (handle_) {
    DisconnectNamedPipe(handle_);
    CloseHandle(handle_);
    handle_ = nullptr;
  }
}

Result<void> PipeClient::connect(const std::string& pipeName) {
  if (handle_ && handle_ != invalidHandle())
    return Result<void>::fail(Error::make("ipc", 31, "already connected"));
  if (!WaitNamedPipeA(pipeName.c_str(), 5000))
    return Result<void>::fail(Error::make("ipc", 32, "runtime pipe not available"));
  HANDLE h = CreateFileA(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                         OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return Result<void>::fail(Error::make(
        "ipc", 33, "pipe open failed, error " + std::to_string(GetLastError())));
  handle_ = h;
  return Result<void>::ok();
}

Result<void> PipeClient::writeFrame(const std::vector<std::uint8_t>& frame) {
  if (!handle_ || handle_ == invalidHandle())
    return Result<void>::fail(Error::make("ipc", 34, "not connected"));
  return writeExact(handle_, frame.data(), frame.size());
}

Result<std::vector<std::uint8_t>> PipeClient::readFrame() {
  if (!handle_ || handle_ == invalidHandle())
    return Result<std::vector<std::uint8_t>>::fail(Error::make("ipc", 35, "not connected"));
  auto hdr = readExact(handle_, 4);
  if (!hdr.ok()) return hdr;
  const auto* d = hdr.value().data();
  std::uint32_t bodyLen =
      static_cast<std::uint32_t>(d[0]) | (static_cast<std::uint32_t>(d[1]) << 8) |
      (static_cast<std::uint32_t>(d[2]) << 16) | (static_cast<std::uint32_t>(d[3]) << 24);
  if (bodyLen > kIpcMaxMessageBytes)
    return Result<std::vector<std::uint8_t>>::fail(Error::make("ipc", 30, "incoming frame over cap"));
  auto body = readExact(handle_, bodyLen);
  if (!body.ok()) return body;
  std::vector<std::uint8_t> frame(4 + bodyLen);
  for (int i = 0; i < 4; ++i) frame[i] = d[i];
  for (std::size_t i = 0; i < bodyLen; ++i) frame[4 + i] = body.value()[i];
  return Result<std::vector<std::uint8_t>>::ok(std::move(frame));
}

void PipeClient::close() {
  if (handle_ && handle_ != invalidHandle()) {
    CloseHandle(handle_);
    handle_ = invalidHandle();
  }
}

}  // namespace ipc
}  // namespace sb

#else
// Non-Windows: transport unavailable. Protocol (validate/encode/decode) is
// still fully usable and tested; only the pipe itself is Windows-only.
namespace sb {
namespace ipc {
PipeHandle PipeServer::invalidHandle() { return -1; }
Result<void> PipeServer::create(const std::string&) {
  return Result<void>::fail(Error::make("ipc", 40, "named pipes are Windows-only"));
}
Result<void> PipeServer::accept() {
  return Result<void>::fail(Error::make("ipc", 40, "named pipes are Windows-only"));
}
Result<bool> PipeServer::acceptWithTimeout(unsigned) {
  return Result<bool>::fail(Error::make("ipc", 40, "named pipes are Windows-only"));
}
Result<void> PipeServer::writeFrame(const std::vector<std::uint8_t>&) {
  return Result<void>::fail(Error::make("ipc", 40, "named pipes are Windows-only"));
}
Result<std::vector<std::uint8_t>> PipeServer::readFrame() {
  return Result<std::vector<std::uint8_t>>::fail(Error::make("ipc", 40, "named pipes are Windows-only"));
}
void PipeServer::close() {}
Result<void> PipeClient::connect(const std::string&) {
  return Result<void>::fail(Error::make("ipc", 40, "named pipes are Windows-only"));
}
Result<void> PipeClient::writeFrame(const std::vector<std::uint8_t>&) {
  return Result<void>::fail(Error::make("ipc", 40, "named pipes are Windows-only"));
}
Result<std::vector<std::uint8_t>> PipeClient::readFrame() {
  return Result<std::vector<std::uint8_t>>::fail(Error::make("ipc", 40, "named pipes are Windows-only"));
}
void PipeClient::close() {}
}  // namespace ipc
}  // namespace sb
#endif
