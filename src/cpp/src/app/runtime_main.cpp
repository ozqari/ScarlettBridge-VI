// scarlettbridge-runtime: owns the compatibility runtime + graphics device.
// Emits versioned IPC events to the UI over a local named pipe (--pipe) and
// always to stdout JSON-lines (debugging). A crash is reported as a
// structured CrashReport; the UI is never taken down by a runtime fault.
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "app/cli_parse.h"
#include "core/config.h"
#include "core/hash.h"
#include "core/logging.h"
#include "core/version.h"
#include "graphics/abstraction.h"
#include "ipc/named_pipe.h"
#include "ipc/protocol.h"
#include "runtime/imports.h"
#include "runtime/lifecycle.h"
#include "runtime/sbm.h"
#include "telemetry/metrics.h"

namespace {

// Publishes each event to stdout and, once a pipe client connects, to the
// pipe. The run loop polls acceptWithTimeout(0) so no thread ever blocks on
// a client and no worker outlives shutdown. A lost client drops back to
// stdio-only with a warning.
class Publisher {
 public:
  void publish(const sb::json::Value& v) {
    std::cout << v.serialize() << "\n";
    std::lock_guard<std::mutex> lock(mu_);
    if (server_ && connected_) {
      if (auto enc = sb::ipc::encode(v); enc.ok()) {
        if (auto w = server_->writeFrame(enc.value()); !w.ok()) {
          sb::log::warn("ipc", "pipe client lost: " + w.error().message);
          connected_ = false;
        }
      }
    }
  }
  void setServer(std::unique_ptr<sb::ipc::PipeServer> s) {
    std::lock_guard<std::mutex> lock(mu_);
    server_ = std::move(s);
  }
  void pollAccept() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!server_ || connected_) return;
    if (auto a = server_->acceptWithTimeout(0); a.ok() && a.value()) {
      connected_ = true;
      sb::log::info("ipc", "UI client connected");
    }
  }

 private:
  std::mutex mu_;
  std::unique_ptr<sb::ipc::PipeServer> server_;
  bool connected_ = false;
};

Publisher g_pub;

void emitState(const sb::Lifecycle& lc) {
  sb::json::Value m = sb::json::Value::object();
  m.set("v", sb::json::Value::number(sb::kIpcProtocolVersion));
  m.set("type", sb::json::Value::str("RuntimeState"));
  m.set("state", sb::json::Value::str(sb::toString(lc.state())));
  m.set("reason", sb::json::Value::str(lc.reason()));
  g_pub.publish(m);
}

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
LONG WINAPI unhandledFilter(EXCEPTION_POINTERS* info) {
  DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
  const void* addr =
      info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;
  std::string detail = sb::diag::formatFaultReport(code, addr, "scarlettbridge-runtime");
  sb::log::error("runtime", "unhandled exception: " + detail);
  sb::diag::CrashReport crash;
  crash.id = "CR-" + sb::fnv1a32hex(detail);
  crash.timestamp = "unhandled-exception";
  crash.reason = detail;
  crash.runtimeVersion = sb::kBuildId;
  std::cerr << crash.toJson(/*redactPaths=*/true) << "\n";
  return EXCEPTION_EXECUTE_HANDLER;
}
#endif

}  // namespace

int main(int argc, char** argv) {
  std::string configPath = "config/default-settings.json";
  std::string pipeName;
  bool stdio = false;
  {
    std::vector<std::string> rest(argv + 1, argv + argc);
    sb::cli::Parser p(rest);
    while (auto a = p.nextArg()) {
      if (*a == "--config" || *a == "--pipe") {
        auto v = p.value(*a);
        if (!v.ok()) {
          std::cerr << "scarlettbridge-runtime: " << v.error().message << "\n";
          return 2;
        }
        if (*a == "--config")
          configPath = v.value();
        else
          pipeName = v.value();
      } else if (*a == "--stdio") {
        stdio = true;
      } else {
        std::cerr << "scarlettbridge-runtime: " << sb::cli::Parser::unexpected(*a) << "\n";
        return 2;
      }
    }
  }
#if defined(_WIN32)
  SetUnhandledExceptionFilter(unhandledFilter);
#endif
  sb::log::init("logs", sb::log::Severity::Info, sb::log::Severity::Debug);
  auto cfg = sb::loadFile(configPath);
  if (!cfg.ok()) {
    sb::log::error("runtime", "config load failed: " + cfg.error().message);
    return 2;
  }
  for (const auto& w : cfg.value().warnings) sb::log::warn("runtime", w);

  if (!pipeName.empty()) {
    auto s = std::make_unique<sb::ipc::PipeServer>();
    if (auto c = s->create(pipeName); !c.ok()) {
      sb::log::error("ipc", c.error().message + " (continuing stdio-only)");
    } else {
      sb::log::info("ipc", "pipe created, polling for UI client");
      g_pub.setServer(std::move(s));
    }
  }

  sb::Lifecycle lc;
  auto step = [&](sb::RuntimeState to, const std::string& why) {
    if (auto r = lc.transition(to, why); !r.ok()) {
      sb::log::error("runtime", r.error().message);
      return false;
    }
    sb::log::info("runtime", "state -> " + sb::toString(to) + " (" + why + ")");
    emitState(lc);
    return true;
  };

  g_pub.publish(sb::ipc::hello(sb::kBuildId));
  if (!step(sb::RuntimeState::Validating, "config v3")) return 1;
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  if (!step(sb::RuntimeState::InitializingPlatform, "host services")) return 1;

  sb::ImportRegistry imports;
  imports.registerService("sb.memory.v1", 1);
  imports.registerService("sb.clock.v1", 1);
  imports.registerService("sb.sync.v1", 1);
  imports.registerService("sb.file.readonly.v1", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  if (!step(sb::RuntimeState::InitializingGraphics, "D3D12")) return 1;

  auto backend = sb::gfx::d3d12Backend();
  if (!backend.ok()) {
    sb::log::error("graphics", backend.error().message);
    lc.transition(sb::RuntimeState::Failed, backend.error().message);
    emitState(lc);
    return 1;
  }
  auto adapters = backend.value()->enumerateAdapters(false);
  if (!adapters.ok()) {
    sb::log::error("graphics", adapters.error().message);
    lc.transition(sb::RuntimeState::Failed, adapters.error().message);
    emitState(lc);
    return 1;
  }
  {
    sb::json::Value m = sb::json::Value::object();
    m.set("v", sb::json::Value::number(sb::kIpcProtocolVersion));
    m.set("type", sb::json::Value::str("HardwareInfo"));
    m.set("adapters", sb::json::Value::number(static_cast<double>(adapters.value().size())));
    g_pub.publish(m);
  }
  if (auto d = backend.value()->createDevice(0, cfg.value().settings.debugLayer,
                                            cfg.value().settings.dred);
      !d.ok()) {
    sb::log::error("graphics", d.error().message);
    lc.transition(sb::RuntimeState::Failed, d.error().message);
    emitState(lc);
    return 1;
  }
  if (!step(sb::RuntimeState::LoadingModules, "synthetic manifest")) return 1;
  if (!step(sb::RuntimeState::Ready, "0 unresolved imports")) return 1;
  if (!step(sb::RuntimeState::Running, "synthetic workload (no game code)")) return 1;

  sb::log::info("runtime", stdio ? "running (stdio mode, Ctrl+C to stop)"
                                 : "running (publish: stdout + pipe when connected)");
  for (int i = 0; i < 3600; ++i) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    g_pub.pollAccept();
    sb::json::Value m = sb::json::Value::object();
    m.set("v", sb::json::Value::number(sb::kIpcProtocolVersion));
    m.set("type", sb::json::Value::str("FrameStats"));
    m.set("cpuMs", sb::json::Value::number(1.5));
    m.set("gpuMs", sb::json::Value::number(4.0));
    g_pub.publish(m);
  }
  backend.value()->destroyDevice();
  lc.transition(sb::RuntimeState::Stopping, "run loop ended");
  lc.transition(sb::RuntimeState::Stopped, "clean");
  emitState(lc);
  sb::log::shutdown();
  return 0;
}
