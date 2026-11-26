#pragma once
// Local Windows named-pipe transport. Current-user-only ACL, fail-closed:
// if the restrictive security descriptor cannot be built, no pipe is
// created. No network listener exists anywhere in this project.
#include <cstdint>
#include <string>
#include <vector>
#include "core/json.h"
#include "core/result.h"

namespace sb {
namespace ipc {

#if defined(_WIN32)
using PipeHandle = void*;
#else
using PipeHandle = int;
#endif

class PipeServer {
 public:
  static PipeHandle invalidHandle();
  PipeServer() = default;
  PipeServer(const PipeServer&) = delete;
  PipeServer& operator=(const PipeServer&) = delete;
  ~PipeServer() { close(); }
  Result<void> create(const std::string& pipeName);
  Result<void> accept();  // blocks for one client
  // Poll for one client without blocking longer than ms. Returns true when a
  // client is now connected. Safe to call repeatedly; a timed-out wait is
  // cancelled and the pipe returns to the listening state.
  Result<bool> acceptWithTimeout(unsigned ms);
  Result<void> writeFrame(const std::vector<std::uint8_t>& frame);
  // Reads one length-prefixed frame (blocking). Enforces the 64 KiB cap.
  Result<std::vector<std::uint8_t>> readFrame();
  void close();

 private:
  PipeHandle handle_ = invalidHandle();
};

class PipeClient {
 public:
  PipeClient() = default;
  PipeClient(const PipeClient&) = delete;
  PipeClient& operator=(const PipeClient&) = delete;
  ~PipeClient() { close(); }
  Result<void> connect(const std::string& pipeName);
  Result<void> writeFrame(const std::vector<std::uint8_t>& frame);
  Result<std::vector<std::uint8_t>> readFrame();
  void close();

 private:
  PipeHandle handle_ = PipeServer::invalidHandle();
};

}  // namespace ipc
}  // namespace sb
