#include "ui/ipc_client.h"

#include "core/version.h"
#include "ipc/named_pipe.h"
#include "ipc/protocol.h"

IpcClientWorker::IpcClientWorker(const QString& pipeName, QObject* parent)
    : QObject(parent), pipeName_(pipeName) {}

void IpcClientWorker::run() {
#if defined(_WIN32)
  sb::ipc::PipeClient client;
  if (auto c = client.connect(pipeName_.toStdString()); !c.ok()) {
    emit disconnected(QString::fromStdString(c.error().message));
    emit finished();
    return;
  }
  emit connected();
  int failures = 0;
  while (!stop_ && failures < 5) {
    auto fr = client.readFrame();
    if (!fr.ok()) {
      ++failures;
      continue;
    }
    failures = 0;
    auto msg = sb::ipc::decode(fr.value().data(), fr.value().size());
    if (!msg.ok()) continue;  // malformed frame: skip, never apply
    emit frameReceived(QByteArray(msg.value().serialize().c_str()));
  }
  client.close();
  emit disconnected(failures >= 5 ? "pipe read failed repeatedly" : "stopped");
#else
  emit disconnected("named-pipe transport is Windows-only");
#endif
  emit finished();
}

void IpcClientWorker::stop() { stop_ = true; }

IpcClient::IpcClient(QObject* parent) : QObject(parent) {}

IpcClient::~IpcClient() { disconnectFromRuntime(); }

bool IpcClient::connectToRuntime(const QString& pipeName) {
#if !defined(_WIN32)
  (void)pipeName;
  return false;
#else
  if (thread_) return true;
  thread_ = new QThread(this);
  worker_ = new IpcClientWorker(pipeName);
  worker_->moveToThread(thread_);
  connect(thread_, &QThread::started, worker_, &IpcClientWorker::run);
  connect(worker_, &IpcClientWorker::finished, thread_, &QThread::quit);
  connect(worker_, &IpcClientWorker::finished, worker_, &QObject::deleteLater);
  connect(thread_, &QThread::finished, thread_, &QObject::deleteLater);
  connect(worker_, &IpcClientWorker::frameReceived, this, &IpcClient::frameReceived);
  connect(worker_, &IpcClientWorker::connected, this, [this] {
    live_ = true;
    emit liveConnected();
  });
  connect(worker_, &IpcClientWorker::disconnected, this, [this](const QString& r) {
    live_ = false;
    emit liveDisconnected(r);
  });
  thread_->start();
  return true;
#endif
}

void IpcClient::disconnectFromRuntime() {
  if (!thread_) return;
  worker_->stop();
  // The runtime heartbeats at 1 Hz, so a blocking pipe read unblocks within
  // about a frame period; the wait cap is a backstop, not the mechanism.
  thread_->quit();
  thread_->wait(5000);
  thread_ = nullptr;
  worker_ = nullptr;
  live_ = false;
}
