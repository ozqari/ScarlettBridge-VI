#pragma once
// Live IPC client for the Qt shell: connects to scarlettbridge-runtime's
// named pipe on a worker thread and emits one signal per validated frame.
// Protocol validation is shared (sb::ipc::decode); malformed frames are
// reported and skipped, never applied. Windows-only transport: on other
// platforms (or with no runtime present) connectToRuntime() reports
// Unsupported and the shell stays in synthetic DEMO mode.
#include <atomic>
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QThread>

class IpcClientWorker : public QObject {
  Q_OBJECT
 public:
  explicit IpcClientWorker(const QString& pipeName, QObject* parent = nullptr);

 public slots:
  void run();   // blocking read loop; emits finished() to stop the thread
  void stop();

 signals:
  void frameReceived(const QByteArray& json);
  void connected();
  void disconnected(const QString& reason);
  void finished();

 private:
  QString pipeName_;
  std::atomic<bool> stop_{false};
};

class IpcClient : public QObject {
  Q_OBJECT
 public:
  explicit IpcClient(QObject* parent = nullptr);
  ~IpcClient() override;
  // Non-blocking: returns false immediately when the transport is
  // unavailable (non-Windows) — caller keeps synthetic mode.
  bool connectToRuntime(const QString& pipeName);
  void disconnectFromRuntime();
  bool isLive() const { return live_; }

 signals:
  void frameReceived(const QByteArray& json);
  void liveConnected();
  void liveDisconnected(const QString& reason);

 private:
  QThread* thread_ = nullptr;
  IpcClientWorker* worker_ = nullptr;
  bool live_ = false;
};
