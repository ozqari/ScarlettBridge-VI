#pragma once
// Qt Widgets control-center shell. Builds only with Qt6; the rest of the
// tree never depends on Qt. Pages: dashboard (lifecycle + system + versions),
// setup (real read-only folder scan + redacted text export), graphics settings (full schema-v3
// load/edit/save via core config), shader cache (real fingerprint + real
// read-only entry/byte stats vs budgets + failure tally + CLI pointers), diagnostics (refusal census + filterable counters/trace ring
// with redacted JSON export outside the repo + live engine-instance probe; no
// live engine figures, OS collection stays unimplemented), logs (filterable sink with
// redacted text export), controls (real enumeration, translation pending),
// scripted DEMO input on the controls page (parse + playback at the synthetic
// tick; live translation stays Unsupported, no device, no presenter),
// benchmark (real in-process D3D12 smoke with capability skips + honest
// single-sample frame-rate readout, sustained FPS never claimed), about (real
// versions). Synthetic event source is always labelled DEMO; live pipe frames
// are LIVE.
#include <QByteArray>
#include <QMainWindow>
#include <QString>
#include "core/config.h"
#include "graphics/shader_tally.h"
#include "media/input_script.h"
#include "runtime/lifecycle.h"
#include "telemetry/metrics.h"
#include "telemetry/refusal_census.h"
#include "ui/backend_status.h"

class QStackedWidget;
class QLabel;
class QPlainTextEdit;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QHBoxLayout;
class IpcClient;

class UiShell : public QMainWindow {
  Q_OBJECT
 public:
  explicit UiShell(QWidget* parent = nullptr);

 private slots:
  void onNav(int row);
  void onStartSynthetic();
  void onStopSynthetic();
  void onCrashSynthetic();
  void onConnectLive();
  void onLiveFrame(const QByteArray& json);
  void onTick();
  void onBrowseFolder();
  void onScanFolder();
  void onExportScan();
  void onRefreshPads();
  void onParseScript();
  void onRunBenchmark();
  void onRefreshCacheStats();
  void onClearDiagnostics();
  void onDiagFilterChanged(const QString& text);
  void onEngineProbeChanged(const QString& text);
  void onExportDiagnostics();
  void onLogFilterChanged(const QString& text);
  void onExportLogs();
  void onClearLogs();
  void onGfxLoad();
  void onGfxSave();
  void onGfxResetDefaults();
  void onGfxAdapterTextChanged(const QString&);

 private:
  void buildTopBar(QHBoxLayout* top);
  void buildPages();
  void buildGfxPage(QWidget* parent);
  void logLine(const QString& sev, const QString& sys, const QString& msg);
  void pushTrace(const std::string& sys, const std::string& msg);
  void refreshDiagnostics();
  void refreshLogs();
  void refreshDashboard();
  bool driveToRunning();
  void setTopState();

  QStackedWidget* stack_ = nullptr;
  QLabel* stateLabel_ = nullptr;
  QLabel* gpuLabel_ = nullptr;
  QLabel* modeLabel_ = nullptr;
  // Dashboard (real).
  QLabel* dashState_ = nullptr;
  QLabel* dashSystem_ = nullptr;
  QLabel* dashVersions_ = nullptr;
  // Setup (real scan).
  QLineEdit* folderEdit_ = nullptr;
  QCheckBox* redactCheck_ = nullptr;
  QLabel* folderResult_ = nullptr;
  // Setup scan report (last scan kept for the redacted text export; the
  // report repeats the shown, possibly redacted, path).
  sb::ui::FolderScan lastScan_;
  bool haveScan_ = false;
  QLabel* scanStatus_ = nullptr;
  // Graphics settings (full schema v3 editor with live selector hint).
  QLineEdit* gfxAdapterEdit_ = nullptr;
  QLabel* gfxAdapterHint_ = nullptr;
  QComboBox* gfxBackendCombo_ = nullptr;
  QComboBox* gfxWindowModeCombo_ = nullptr;
  QComboBox* gfxResCombo_ = nullptr;
  QCheckBox* gfxVsyncCheck_ = nullptr;
  QSpinBox* gfxFrameCapSpin_ = nullptr;
  QSpinBox* gfxTexBudgetSpin_ = nullptr;
  QCheckBox* gfxShaderCacheCheck_ = nullptr;
  QSpinBox* gfxCacheMBSpin_ = nullptr;
  QLineEdit* gfxCachePathEdit_ = nullptr;
  QCheckBox* gfxPrecompileCheck_ = nullptr;
  QCheckBox* gfxDebugLayerCheck_ = nullptr;
  QCheckBox* gfxDredCheck_ = nullptr;
  QLabel* gfxStatus_ = nullptr;
  void applySettingsToWidgets(const sb::Settings& s);
  sb::Settings collectSettingsFromWidgets() const;
  void refreshAdapterHint();
  // Diagnostics (filterable ring + export).
  QPlainTextEdit* diagView_ = nullptr;
  QLineEdit* diagFilterEdit_ = nullptr;
  QLabel* diagStatus_ = nullptr;
  // Diagnostics engine-instance probe (pure parse of typed text; no live
  // figures, OS counter collection stays unimplemented).
  QLineEdit* engineProbeEdit_ = nullptr;
  QLabel* engineProbeResult_ = nullptr;
  // Logs (filterable sink + export). logLines_ is the capped filter source.
  QPlainTextEdit* logView_ = nullptr;
  QLineEdit* logFilterEdit_ = nullptr;
  QLabel* logStatus_ = nullptr;
  std::vector<std::string> logLines_;
  // Controls (real enumeration).
  QLabel* padsLabel_ = nullptr;
  QLabel* audioLabel_ = nullptr;
  // Controls scripted input (DEMO: parsed script + playback at the synthetic
  // tick; no device, no presenter).
  QPlainTextEdit* padsScriptEdit_ = nullptr;
  QLabel* padsScriptSummary_ = nullptr;
  QLabel* padsScriptState_ = nullptr;
  sb::input::InputScript padsScript_;
  // Shader manager (real fingerprint + read-only stats).
  QLabel* cacheStats_ = nullptr;
  // Real shader failure accounting (increment 23 backend), surfaced
  // read-only. Zeros mean no batch translator run yet — never faked.
  QLabel* shaderTally_ = nullptr;
  sb::gfx::FailTally shaderFailures_;
  // Benchmark (real in-process smoke with capability skips + an honest
  // single-sample frame-rate readout; sustained FPS is never claimed).
  QLabel* benchResult_ = nullptr;
  QLabel* benchRate_ = nullptr;
  IpcClient* ipc_ = nullptr;
  // Portable backend state owned by the shell (no Qt here).
  sb::Lifecycle lifecycle_;
  sb::diag::Counters counters_;
  sb::diag::TraceRing trace_{256};
  sb::diag::LastOutputRing lastOut_{8};
  // Real refusal accounting (increment 11 telemetry), surfaced read-only in
  // Diagnostics. Zeros mean no translated draw has been refused yet — the
  // synthetic ticker binds no descriptors — never a faked backend.
  sb::diag::NullCensus nullCensus_;
  sb::diag::PerReasonCensus refusalCensus_;
  int tick_ = 0;
  bool running_ = false;
};
