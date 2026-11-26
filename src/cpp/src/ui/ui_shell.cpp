#include "ui/ui_shell.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "core/config.h"
#include "core/version.h"
#include "graphics/abstraction.h"
#include "graphics/adapter_select.h"
#include "media/input_script.h"
#include "telemetry/metrics.h"
#include "ui/backend_status.h"
#include "ui/ipc_client.h"

namespace {
QWidget* pendingCard(const QString& title, const QString& reason) {
  auto* w = new QWidget;
  auto* lay = new QVBoxLayout(w);
  auto* h = new QLabel("<b>" + title + "</b>");
  auto* p = new QLabel(reason);
  p->setWordWrap(true);
  lay->addWidget(h);
  lay->addWidget(p);
  lay->addStretch(1);
  return w;
}
QString isoNow() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODate); }
}  // namespace

UiShell::UiShell(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("ScarlettBridge — control center");
  resize(1100, 700);

  auto* central = new QWidget(this);
  auto* root = new QHBoxLayout(central);
  root->setContentsMargins(0, 0, 0, 0);

  auto* nav = new QListWidget(central);
  nav->setFixedWidth(210);
  const char* pages[] = {"Dashboard",   "Local Content Setup", "Graphics Settings",
                         "Shader Manager", "Live Diagnostics", "Console + Logs",
                         "Controls",      "Hardware Benchmark", "About / Updates"};
  for (const char* p : pages) nav->addItem(p);
  root->addWidget(nav);

  auto* right = new QWidget(central);
  auto* rlay = new QVBoxLayout(right);
  rlay->setContentsMargins(12, 8, 12, 12);
  auto* top = new QHBoxLayout;
  buildTopBar(top);
  rlay->addLayout(top);

  stack_ = new QStackedWidget(right);
  rlay->addWidget(stack_, 1);
  root->addWidget(central, 1);
  setCentralWidget(central);

  buildPages();

  connect(nav, &QListWidget::currentRowChanged, this, &UiShell::onNav);
  nav->setCurrentRow(0);

  ipc_ = new IpcClient(this);
  connect(ipc_, &IpcClient::frameReceived, this, &UiShell::onLiveFrame);
  connect(ipc_, &IpcClient::liveConnected, this, [this] {
    modeLabel_->setText("LIVE");
    logLine("INFO", "ipc", "live runtime connected");
    pushTrace("ipc", "live runtime connected");
  });
  connect(ipc_, &IpcClient::liveDisconnected, this, [this](const QString& r) {
    modeLabel_->setText("DEMO DATA");
    logLine("WARN", "ipc", "live runtime disconnected: " + r);
    pushTrace("ipc", "live disconnected");
  });

  auto* timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &UiShell::onTick);
  timer->start(1000);

  refreshDashboard();
  logLine("INFO", "ui", "Qt shell boot (IPC v1, schema v3)");
  pushTrace("ui", "shell boot");
}

void UiShell::buildTopBar(QHBoxLayout* top) {
  stateLabel_ = new QLabel("Created");
  gpuLabel_ = new QLabel(QString::fromStdString(sb::ui::adapterLine()).left(90));
  gpuLabel_->setToolTip(QString::fromStdString(sb::ui::adapterLine()));
  auto* ver = new QLabel("v0.1.0-cpp");
  modeLabel_ = new QLabel("DEMO DATA");
  auto* startBtn = new QPushButton("Start synthetic");
  auto* stopBtn = new QPushButton("Stop");
  auto* crashBtn = new QPushButton("Crash (synthetic)");
  auto* liveBtn = new QPushButton("Connect live");
  top->addWidget(stateLabel_);
  top->addWidget(gpuLabel_);
  top->addWidget(ver);
  top->addWidget(modeLabel_);
  top->addStretch(1);
  top->addWidget(startBtn);
  top->addWidget(stopBtn);
  top->addWidget(crashBtn);
  top->addWidget(liveBtn);
  connect(startBtn, &QPushButton::clicked, this, &UiShell::onStartSynthetic);
  connect(stopBtn, &QPushButton::clicked, this, &UiShell::onStopSynthetic);
  connect(crashBtn, &QPushButton::clicked, this, &UiShell::onCrashSynthetic);
  connect(liveBtn, &QPushButton::clicked, this, &UiShell::onConnectLive);
}

void UiShell::buildPages() {
  // 0 Dashboard (real: lifecycle + system + versions).
  {
    auto* w = new QWidget;
    auto* form = new QFormLayout(w);
    dashState_ = new QLabel("Created");
    dashSystem_ = new QLabel(QString::fromStdString(sb::ui::systemLine()));
    dashSystem_->setWordWrap(true);
    const sb::ui::Versions v = sb::ui::versions();
    dashVersions_ = new QLabel(QString("app %1 build %2 ipc %3 schema %4")
                                   .arg(QString::fromStdString(v.app),
                                        QString::fromStdString(v.build),
                                        QString::fromStdString(v.ipc),
                                        QString::fromStdString(v.schema)));
    auto* ev = new QLabel("synthetic only — no playable claims");
    form->addRow("Runtime state:", dashState_);
    form->addRow("System:", dashSystem_);
    form->addRow("Versions:", dashVersions_);
    form->addRow("Evidence:", ev);
    stack_->addWidget(w);
  }
  // 1 Setup (real read-only scan).
  {
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    auto* row = new QHBoxLayout;
    folderEdit_ = new QLineEdit;
    folderEdit_->setPlaceholderText("Path to a readable folder (never modified)");
    auto* browse = new QPushButton("Browse…");
    row->addWidget(folderEdit_, 1);
    row->addWidget(browse);
    lay->addLayout(row);
    redactCheck_ = new QCheckBox("Redact path in output");
    redactCheck_->setChecked(true);
    auto* scan = new QPushButton("Scan (read-only, capped)");
    folderResult_ = new QLabel("No scan yet.");
    folderResult_->setWordWrap(true);
    auto* note = new QLabel(
        "Read-only: names/sizes + SHA-256 of the first 64 KiB for files under 5 MiB "
        "(max 40 hashed, 500 entries). Never writes into the folder. "
        "CLI parity: scarlettbridge-cli validate-folder <dir> [--redact].");
    note->setWordWrap(true);
    auto* exp = new QPushButton("Export scan report…");
    scanStatus_ = new QLabel("The report repeats the shown (possibly redacted) path and "
                             "refuses in-tree destinations.");
    scanStatus_->setWordWrap(true);
    lay->addWidget(redactCheck_);
    lay->addWidget(scan);
    lay->addWidget(exp);
    lay->addWidget(folderResult_);
    lay->addWidget(note);
    lay->addWidget(scanStatus_);
    lay->addStretch(1);
    connect(browse, &QPushButton::clicked, this, &UiShell::onBrowseFolder);
    connect(scan, &QPushButton::clicked, this, &UiShell::onScanFolder);
    connect(exp, &QPushButton::clicked, this, &UiShell::onExportScan);
    stack_->addWidget(w);
  }
  // 2 Graphics settings (full schema-v3 editor; built by buildGfxPage).
  {
    auto* w = new QWidget;
    buildGfxPage(w);
    stack_->addWidget(w);
  }
  // 3 Shader manager (real fingerprint + real read-only stats + CLI pointers).
  {
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    auto* form = new QFormLayout;
    auto* fp = new QLabel(QString::fromStdString(sb::ui::cacheFingerprint()));
    fp->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cacheStats_ = new QLabel("Not scanned. Read-only: counts *.bin entries, never writes.");
    cacheStats_->setWordWrap(true);
    cacheStats_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* refresh = new QPushButton("Refresh cache stats");
    shaderTally_ = new QLabel("No shader batch failures recorded yet.", w);
    shaderTally_->setWordWrap(true);
    shaderTally_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* tallyNote = new QLabel(
        "Failure tally is empty until a batch translator run records one (none exists "
        "in this tree — zeros are honest, not a backend).", w);
    tallyNote->setWordWrap(true);
    auto* q = new QLabel(
        "Compile queue binds to sb_graphics + cache in a later phase. Today: "
        "scarlettbridge-cli translate <sbvs> | sb_shader_probe | sb_cache_inspector.");
    q->setWordWrap(true);
    form->addRow("Translator fingerprint:", fp);
    form->addRow("Cache stats:", cacheStats_);
    lay->addLayout(form);
    lay->addWidget(refresh);
    lay->addWidget(shaderTally_);
    lay->addWidget(tallyNote);
    lay->addWidget(q);
    lay->addStretch(1);
    connect(refresh, &QPushButton::clicked, this, &UiShell::onRefreshCacheStats);
    stack_->addWidget(w);
    onRefreshCacheStats();
  }
  // 4 Live Diagnostics (filterable counters/trace ring + guarded export).
  {
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    auto* engineNote = new QLabel(
        "Engines (no live figures — OS counter collection is not implemented):", w);
    engineProbeEdit_ = new QLineEdit(w);
    engineProbeEdit_->setPlaceholderText("pid_<pid>_..._engtype_<kind>");
    engineProbeEdit_->setText(
        "pid_1234_luid_0x00000000_0x0000E3B5_phys_0_eng_0_engtype_3D");
    engineProbeEdit_->setClearButtonEnabled(true);
    engineProbeResult_ = new QLabel(w);
    engineProbeResult_->setWordWrap(true);
    engineProbeResult_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(engineNote);
    lay->addWidget(engineProbeEdit_);
    lay->addWidget(engineProbeResult_);
    diagFilterEdit_ = new QLineEdit(w);
    diagFilterEdit_->setPlaceholderText("Filter (case-insensitive substring, empty = all)");
    diagFilterEdit_->setClearButtonEnabled(true);
    diagView_ = new QPlainTextEdit(w);
    diagView_->setReadOnly(true);
    diagView_->setMaximumBlockCount(2000);
    diagView_->setPlaceholderText(
        "Refusals + counters + recent trace (synthetic ticker + LIVE frames).");
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    auto* clear = new QPushButton("Clear view", btnRow);
    auto* exp = new QPushButton("Export redacted JSON…", btnRow);
    btnLay->addWidget(clear);
    btnLay->addWidget(exp);
    diagStatus_ = new QLabel("Ring keeps history; export refuses in-tree paths.", w);
    diagStatus_->setWordWrap(true);
    lay->addWidget(diagFilterEdit_);
    lay->addWidget(diagView_, 1);
    lay->addWidget(btnRow);
    lay->addWidget(diagStatus_);
    connect(clear, &QPushButton::clicked, this, &UiShell::onClearDiagnostics);
    connect(exp, &QPushButton::clicked, this, &UiShell::onExportDiagnostics);
    connect(diagFilterEdit_, &QLineEdit::textChanged, this,
            &UiShell::onDiagFilterChanged);
    connect(engineProbeEdit_, &QLineEdit::textChanged, this,
            &UiShell::onEngineProbeChanged);
    onEngineProbeChanged(engineProbeEdit_->text());
    stack_->addWidget(w);
  }
  // 5 Logs (filterable sink + guarded redacted export).
  {
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    logFilterEdit_ = new QLineEdit(w);
    logFilterEdit_->setPlaceholderText("Filter (case-insensitive substring, empty = all)");
    logFilterEdit_->setClearButtonEnabled(true);
    logView_ = new QPlainTextEdit(w);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(2000);
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    auto* clear = new QPushButton("Clear view", btnRow);
    auto* exp = new QPushButton("Export redacted text…", btnRow);
    btnLay->addWidget(clear);
    btnLay->addWidget(exp);
    logStatus_ = new QLabel("Exports redact user paths and refuse in-tree paths.", w);
    logStatus_->setWordWrap(true);
    lay->addWidget(logFilterEdit_);
    lay->addWidget(logView_, 1);
    lay->addWidget(btnRow);
    lay->addWidget(logStatus_);
    connect(clear, &QPushButton::clicked, this, &UiShell::onClearLogs);
    connect(exp, &QPushButton::clicked, this, &UiShell::onExportLogs);
    connect(logFilterEdit_, &QLineEdit::textChanged, this,
            &UiShell::onLogFilterChanged);
    stack_->addWidget(w);
  }
  // 6 Controls (real enumeration; translation pending).
  {
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    padsLabel_ = new QLabel(QString::fromStdString(sb::ui::gamepadLine()));
    padsLabel_->setWordWrap(true);
    audioLabel_ = new QLabel(QString::fromStdString(sb::ui::audioLine()));
    audioLabel_->setWordWrap(true);
    auto* note = new QLabel(
        "Live event translation is Unsupported until the Windows backend lands "
        "with tests (media/input_backend pollOnce). Below is DEMO scripted "
        "input only: parsed by media/input_script, played at the synthetic "
        "tick, no device, no presenter.");
    note->setWordWrap(true);
    auto* refresh = new QPushButton("Refresh enumeration");
    auto* scriptTitle = new QLabel("<b>Scripted input (DEMO, no device)</b>");
    padsScriptEdit_ = new QPlainTextEdit;
    padsScriptEdit_->setPlainText("1@0+0.3, 2@0.5, lt=1@1.0");
    padsScriptEdit_->setPlaceholderText(
        "<hex>@<s>[+<d>], lt/rt in [0,1], lx/ly/rx/ry in [-1,1], base=poll|start "
        "first; waits/marks/groups stay rejections (need a presenter)");
    padsScriptEdit_->setMaximumBlockCount(200);
    auto* parse = new QPushButton("Parse script (DEMO)");
    padsScriptSummary_ = new QLabel("No script parsed yet.");
    padsScriptSummary_->setWordWrap(true);
    padsScriptSummary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    padsScriptState_ = new QLabel(
        "pad: no script (press Parse, then Start synthetic to play at the tick).");
    padsScriptState_->setWordWrap(true);
    padsScriptState_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(padsLabel_);
    lay->addWidget(audioLabel_);
    lay->addWidget(note);
    lay->addWidget(refresh);
    lay->addWidget(scriptTitle);
    lay->addWidget(padsScriptEdit_);
    lay->addWidget(parse);
    lay->addWidget(padsScriptSummary_);
    lay->addWidget(padsScriptState_);
    lay->addStretch(1);
    connect(refresh, &QPushButton::clicked, this, &UiShell::onRefreshPads);
    connect(parse, &QPushButton::clicked, this, &UiShell::onParseScript);
    stack_->addWidget(w);
  }
  // 7 Benchmark (real in-process smoke, capability skips honest).
  {
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    benchResult_ = new QLabel("Not run. In-process D3D12 offscreen smoke; WARP fallback.");
    benchResult_->setWordWrap(true);
    benchRate_ = new QLabel("No frame-rate sample yet (needs one successful smoke run).");
    benchRate_->setWordWrap(true);
    benchRate_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* run = new QPushButton("Run smoke (may report SKIP without D3D12)");
    auto* note = new QLabel("Headless parity: sb_gpu_probe [--warp] [--out <outside-repo path>]. "
                            "The rate below is one one-shot draw, never a sustained FPS claim.");
    note->setWordWrap(true);
    lay->addWidget(benchResult_);
    lay->addWidget(benchRate_);
    lay->addWidget(run);
    lay->addWidget(note);
    lay->addStretch(1);
    connect(run, &QPushButton::clicked, this, &UiShell::onRunBenchmark);
    stack_->addWidget(w);
  }
  // 8 About (real versions).
  {
    const sb::ui::Versions v = sb::ui::versions();
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    auto* h = new QLabel("<b>ScarlettBridge 0.1.0-cpp</b>");
    auto* p = new QLabel(QString("app %1 build %2 | ipc v%3 schema v%4 | translator v%5 "
                                 "cache v%6 sbm v%7. No upstream code vendored; see "
                                 "THIRD_PARTY_NOTICES.md and src/docs.")
                             .arg(QString::fromStdString(v.app),
                                  QString::fromStdString(v.build),
                                  QString::fromStdString(v.ipc),
                                  QString::fromStdString(v.schema),
                                  QString::fromStdString(v.translator),
                                  QString::fromStdString(v.cache),
                                  QString::fromStdString(v.sbm)));
    p->setWordWrap(true);
    lay->addWidget(h);
    lay->addWidget(p);
    lay->addStretch(1);
    stack_->addWidget(w);
  }
}

void UiShell::buildGfxPage(QWidget* parent) {
  auto* form = new QFormLayout(parent);
  gfxAdapterEdit_ = new QLineEdit(parent);
  gfxAdapterEdit_->setPlaceholderText("auto | warp | index | model | name part");
  gfxAdapterHint_ = new QLabel(parent);
  gfxAdapterHint_->setWordWrap(true);
  gfxAdapterHint_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  gfxBackendCombo_ = new QComboBox(parent);
  for (const auto& b : sb::ui::settingBackends())
    gfxBackendCombo_->addItem(QString::fromStdString(b));
  gfxBackendCombo_->setEnabled(false);  // only the tested backend stays selectable
  gfxWindowModeCombo_ = new QComboBox(parent);
  for (const auto& m : sb::ui::settingWindowModes())
    gfxWindowModeCombo_->addItem(QString::fromStdString(m));
  gfxResCombo_ = new QComboBox(parent);
  for (const auto& r : sb::ui::settingResolutions())
    gfxResCombo_->addItem(QString::fromStdString(r));
  gfxVsyncCheck_ = new QCheckBox("VSync", parent);
  gfxFrameCapSpin_ = new QSpinBox(parent);
  gfxFrameCapSpin_->setRange(0, 1000);
  gfxTexBudgetSpin_ = new QSpinBox(parent);
  gfxTexBudgetSpin_->setRange(256, 32768);
  gfxTexBudgetSpin_->setSingleStep(256);
  gfxShaderCacheCheck_ = new QCheckBox("Shader cache", parent);
  gfxCacheMBSpin_ = new QSpinBox(parent);
  gfxCacheMBSpin_->setRange(16, 32768);
  gfxCachePathEdit_ = new QLineEdit(parent);
  gfxCachePathEdit_->setPlaceholderText("%LOCALAPPDATA%/scarlettbridge/shader-cache");
  gfxPrecompileCheck_ = new QCheckBox("Precompile", parent);
  gfxDebugLayerCheck_ = new QCheckBox("Debug layer", parent);
  gfxDredCheck_ = new QCheckBox("DRED", parent);
  auto* btnRow = new QWidget(parent);
  auto* btnLay = new QHBoxLayout(btnRow);
  btnLay->setContentsMargins(0, 0, 0, 0);
  auto* load = new QPushButton("Reload", btnRow);
  auto* save = new QPushButton("Save", btnRow);
  auto* reset = new QPushButton("Defaults", btnRow);
  btnLay->addWidget(load);
  btnLay->addWidget(save);
  btnLay->addWidget(reset);
  gfxStatus_ = new QLabel("Only the tested d3d12 backend stays selectable.", parent);
  gfxStatus_->setWordWrap(true);
  form->addRow("Adapter:", gfxAdapterEdit_);
  form->addRow("Selects:", gfxAdapterHint_);
  form->addRow("Backend:", gfxBackendCombo_);
  form->addRow("Window mode:", gfxWindowModeCombo_);
  form->addRow("Resolution:", gfxResCombo_);
  form->addRow("", gfxVsyncCheck_);
  form->addRow("Frame cap (0 = uncapped):", gfxFrameCapSpin_);
  form->addRow("Texture budget (MB):", gfxTexBudgetSpin_);
  form->addRow("", gfxShaderCacheCheck_);
  form->addRow("Cache (MB):", gfxCacheMBSpin_);
  form->addRow("Cache path:", gfxCachePathEdit_);
  form->addRow("", gfxPrecompileCheck_);
  form->addRow("", gfxDebugLayerCheck_);
  form->addRow("", gfxDredCheck_);
  form->addRow("", btnRow);
  form->addRow("Status:", gfxStatus_);
  connect(load, &QPushButton::clicked, this, &UiShell::onGfxLoad);
  connect(save, &QPushButton::clicked, this, &UiShell::onGfxSave);
  connect(reset, &QPushButton::clicked, this, &UiShell::onGfxResetDefaults);
  connect(gfxAdapterEdit_, &QLineEdit::textChanged, this,
          &UiShell::onGfxAdapterTextChanged);
  onGfxLoad();
  refreshAdapterHint();
}

void UiShell::applySettingsToWidgets(const sb::Settings& s) {
  gfxAdapterEdit_->setText(QString::fromStdString(s.adapter));
  gfxBackendCombo_->setCurrentText(QString::fromStdString(s.backend));
  gfxWindowModeCombo_->setCurrentText(QString::fromStdString(s.windowMode));
  gfxResCombo_->setCurrentText(QString::fromStdString(s.resolution));
  gfxVsyncCheck_->setChecked(s.vsync);
  gfxFrameCapSpin_->setValue(s.frameCap);
  gfxTexBudgetSpin_->setValue(s.texBudgetMB);
  gfxShaderCacheCheck_->setChecked(s.shaderCache);
  gfxCacheMBSpin_->setValue(s.cacheMB);
  gfxCachePathEdit_->setText(QString::fromStdString(s.cachePath));
  gfxPrecompileCheck_->setChecked(s.precompile);
  gfxDebugLayerCheck_->setChecked(s.debugLayer);
  gfxDredCheck_->setChecked(s.dred);
}

sb::Settings UiShell::collectSettingsFromWidgets() const {
  sb::Settings s = sb::defaults();
  s.adapter = gfxAdapterEdit_->text().toStdString();
  s.backend = gfxBackendCombo_->currentText().toStdString();
  s.windowMode = gfxWindowModeCombo_->currentText().toStdString();
  s.resolution = gfxResCombo_->currentText().toStdString();
  s.vsync = gfxVsyncCheck_->isChecked();
  s.frameCap = gfxFrameCapSpin_->value();
  s.texBudgetMB = gfxTexBudgetSpin_->value();
  s.shaderCache = gfxShaderCacheCheck_->isChecked();
  s.cacheMB = gfxCacheMBSpin_->value();
  s.cachePath = gfxCachePathEdit_->text().toStdString();
  s.precompile = gfxPrecompileCheck_->isChecked();
  s.debugLayer = gfxDebugLayerCheck_->isChecked();
  s.dred = gfxDredCheck_->isChecked();
  return s;
}

void UiShell::onGfxLoad() {
  auto loaded = sb::loadFile("scarlettbridge-settings.json");
  applySettingsToWidgets(loaded.value().settings);
  QString msg = "Loaded scarlettbridge-settings.json.";
  if (!loaded.value().warnings.empty()) {
    msg += " Corrections:";
    for (const auto& w : loaded.value().warnings)
      msg += " [" + QString::fromStdString(w) + "]";
  }
  gfxStatus_->setText(msg);
  logLine("INFO", "ui", msg.left(400));
}

void UiShell::onGfxSave() {
  sb::Settings s = collectSettingsFromWidgets();
  // Round-trip through the validator so corrections are reported, never
  // silently kept (same rule as the CLI and the web prototype).
  sb::SettingsLoad checked = sb::fromJson(sb::toJson(s));
  applySettingsToWidgets(checked.settings);
  auto r = sb::saveFile("scarlettbridge-settings.json", checked.settings);
  QString msg;
  if (!r.ok()) {
    msg = "Save FAILED: " + QString::fromStdString(r.error().message);
    gfxStatus_->setText(msg);
    logLine("ERROR", "ui", msg);
    return;
  }
  msg = "Saved: " + QString::fromStdString(sb::ui::settingsSummary(checked.settings));
  if (!checked.warnings.empty()) {
    msg += " Corrections:";
    for (const auto& w : checked.warnings) msg += " [" + QString::fromStdString(w) + "]";
  }
  gfxStatus_->setText(msg);
  gfxStatus_->setToolTip(msg);
  counters_.add("settings.saves", 1);
  pushTrace("settings", sb::ui::settingsSummary(checked.settings));
  logLine("INFO", "ui", msg.left(400));
}

void UiShell::onGfxResetDefaults() {
  applySettingsToWidgets(sb::defaults());
  gfxStatus_->setText("Defaults staged (press Save to write).");
  logLine("INFO", "ui", "settings reset to defaults (staged)");
}

void UiShell::onGfxAdapterTextChanged(const QString&) { refreshAdapterHint(); }

void UiShell::refreshAdapterHint() {
  if (!gfxAdapterHint_ || !gfxAdapterEdit_) return;
  const std::string hint = sb::gfx::describeSelector(
      sb::gfx::parseAdapterSelector(gfxAdapterEdit_->text().toStdString()));
  gfxAdapterHint_->setText(QString::fromStdString(hint));
  gfxAdapterHint_->setToolTip(QString::fromStdString(hint));
}

void UiShell::onNav(int row) {
  if (row >= 0 && row < stack_->count()) stack_->setCurrentIndex(row);
}

bool UiShell::driveToRunning() {
  // Fresh instances always start at Created; restarts from Failed/Stopped go
  // through Validating again (see Lifecycle::transition).
  const sb::RuntimeState path[] = {
      sb::RuntimeState::Validating,          sb::RuntimeState::InitializingPlatform,
      sb::RuntimeState::InitializingGraphics, sb::RuntimeState::LoadingModules,
      sb::RuntimeState::Ready,               sb::RuntimeState::Running};
  for (sb::RuntimeState to : path) {
    if (lifecycle_.state() == to) continue;
    auto r = lifecycle_.transition(to, "synthetic (no game code)");
    if (!r.ok()) {
      logLine("WARN", "runtime", QString::fromStdString(r.error().message));
      return false;
    }
    logLine("INFO", "runtime",
            QString("state -> %1 (synthetic workload, no game code)")
                .arg(QString::fromStdString(sb::toString(to))));
  }
  return lifecycle_.state() == sb::RuntimeState::Running;
}

void UiShell::setTopState() {
  const QString s = QString::fromStdString(sb::toString(lifecycle_.state()));
  if (lifecycle_.state() == sb::RuntimeState::Running)
    stateLabel_->setText(running_ ? "Running (synthetic DEMO)" : "Running");
  else
    stateLabel_->setText(s);
  refreshDashboard();
}

void UiShell::refreshDashboard() {
  if (!dashState_) return;
  QString s = QString::fromStdString(sb::toString(lifecycle_.state()));
  if (!lifecycle_.reason().empty()) s += " — " + QString::fromStdString(lifecycle_.reason());
  dashState_->setText(s);
}

void UiShell::onStartSynthetic() {
  running_ = true;
  tick_ = 0;
  if (!driveToRunning()) {
    running_ = false;
    setTopState();
    return;
  }
  counters_.add("synthetic.starts", 1);
  pushTrace("runtime", "state -> Running (synthetic)");
  setTopState();
}

void UiShell::onStopSynthetic() {
  running_ = false;
  auto step = [&](sb::RuntimeState to) {
    if (lifecycle_.state() == to) return true;
    auto r = lifecycle_.transition(to, "clean");
    if (!r.ok()) {
      logLine("WARN", "runtime", QString::fromStdString(r.error().message));
      return false;
    }
    return true;
  };
  if (lifecycle_.state() == sb::RuntimeState::Running) {
    if (step(sb::RuntimeState::Stopping)) step(sb::RuntimeState::Stopped);
  } else if (lifecycle_.state() == sb::RuntimeState::Ready) {
    if (step(sb::RuntimeState::Stopping)) step(sb::RuntimeState::Stopped);
  } else {
    logLine("INFO", "runtime", "stop: nothing running");
  }
  counters_.add("synthetic.stops", 1);
  pushTrace("runtime", "state -> Stopped (clean)");
  setTopState();
}

void UiShell::onCrashSynthetic() {
  running_ = false;
  if (lifecycle_.state() == sb::RuntimeState::Failed) {
    logLine("INFO", "runtime", "already Failed; Start begins a fresh instance");
    setTopState();
    return;
  }
  auto r = lifecycle_.transition(sb::RuntimeState::Failed, "synthetic crash (no game code)");
  if (!r.ok()) {
    logLine("WARN", "runtime", QString::fromStdString(r.error().message));
    return;
  }
  sb::diag::CrashReport crash;
  crash.id = "CR-synthetic-1";
  crash.timestamp = isoNow().toStdString();
  crash.reason = sb::diag::formatFaultReport(0xC0000005ul, nullptr, "synthetic");
  crash.runtimeVersion = sb::kBuildId;
  crash.lastEvents = lastOut_.snapshot();
  logLine("ERROR", "runtime", QString::fromStdString(crash.toJson(/*redactPaths=*/true)));
  counters_.add("synthetic.crashes", 1);
  pushTrace("runtime", "state -> Failed (synthetic)");
  setTopState();
}

void UiShell::onConnectLive() {
  if (ipc_ && ipc_->isLive()) {
    ipc_->disconnectFromRuntime();
    return;
  }
  if (ipc_ && !ipc_->connectToRuntime("\\\\.\\pipe\\scarlettbridge-v1-ui")) {
    logLine("WARN", "ipc", "live transport unavailable (non-Windows); staying synthetic");
    return;
  }
  logLine("INFO", "ipc", "connecting to live runtime…");
}

void UiShell::onLiveFrame(const QByteArray& json) {
  const QString s = QString::fromUtf8(json);
  counters_.add("live.frames", 1);
  pushTrace("ipc-live", s.left(160).toStdString());
  if (s.contains("\"type\":\"RuntimeState\"") || s.contains("\"type\": \"RuntimeState\"")) {
    if (s.contains("Running"))
      stateLabel_->setText("Running (LIVE)");
    else if (s.contains("Failed"))
      stateLabel_->setText("Failed (LIVE)");
  }
  if (s.contains("FrameStats")) counters_.add("live.framestats", 1);
  logLine("DEBUG", "ipc-live", s.left(220));
  refreshDiagnostics();
}

void UiShell::onTick() {
  if (!running_) return;
  ++tick_;
  counters_.add("synthetic.tick", 1);
  pushTrace("graphics", "FrameStats tick=" + std::to_string(tick_) + " (synthetic)");
  if (padsScriptState_ && padsScript_.items > 0) {
    const std::string st =
        sb::ui::scriptStateLine(sb::input::playInputScript(padsScript_, static_cast<double>(tick_)));
    const QString state =
        QString::fromStdString(st + " (DEMO at t=" + std::to_string(tick_) +
                               " s; no device, no presenter)");
    padsScriptState_->setText(state);
    padsScriptState_->setToolTip(state);
  }
  if (tick_ % 5 == 0) {
    logLine("DEBUG", "graphics", QString("FrameStats tick=%1 (synthetic)").arg(tick_));
    refreshDiagnostics();
  }
}

void UiShell::onBrowseFolder() {
  const QString d = QFileDialog::getExistingDirectory(this, "Choose a readable folder");
  if (!d.isEmpty() && folderEdit_) folderEdit_->setText(d);
}

void UiShell::onScanFolder() {
  if (!folderEdit_ || !folderResult_) return;
  const std::string path = folderEdit_->text().toStdString();
  const bool redact = redactCheck_ && redactCheck_->isChecked();
  const sb::ui::FolderScan r = sb::ui::scanFolderReadOnly(path, redact);
  lastScan_ = r;
  haveScan_ = true;
  if (!r.ok) {
    folderResult_->setText(QString("Scan failed: ") + QString::fromStdString(r.error));
    logLine("WARN", "setup", "folder scan failed: " + QString::fromStdString(r.error));
    return;
  }
  folderResult_->setText(QString("dir=%1 entries=%2 bytes=%3 hashed=%4 ms=%5")
                             .arg(QString::fromStdString(r.shownDir))
                             .arg(r.entries)
                             .arg(r.bytes)
                             .arg(r.hashed)
                             .arg(r.ms, 0, 'f', 1));
  counters_.add("setup.scans", 1);
  pushTrace("setup", "scan entries=" + std::to_string(r.entries));
  logLine("INFO", "setup", folderResult_->text());
}

void UiShell::onExportScan() {
  if (!scanStatus_) return;
  if (!haveScan_) {
    scanStatus_->setText("No scan yet — scan first, then export.");
    return;
  }
  const std::string text = sb::ui::folderReportText(lastScan_);
  const QString path = QFileDialog::getSaveFileName(this, "Export scan report (redacted text)",
                                                    "", "Text (*.txt *.log)");
  if (path.isEmpty()) return;
  auto r = sb::ui::exportTextOutsideRepo(path.toStdString(), text);
  if (r.ok()) {
    scanStatus_->setText("Exported scan report to " + path + ".");
    counters_.add("setup.report-exports", 1);
    pushTrace("setup", "scan report exported");
    logLine("INFO", "setup", scanStatus_->text().left(300));
  } else {
    scanStatus_->setText("Export FAILED: " + QString::fromStdString(r.error().message));
    logLine("WARN", "setup", scanStatus_->text().left(300));
  }
}

void UiShell::onRefreshPads() {
  if (!padsLabel_ || !audioLabel_) return;
  padsLabel_->setText(QString::fromStdString(sb::ui::gamepadLine()));
  audioLabel_->setText(QString::fromStdString(sb::ui::audioLine()));
  logLine("INFO", "input", padsLabel_->text().left(220));
}

void UiShell::onParseScript() {
  if (!padsScriptEdit_ || !padsScriptSummary_ || !padsScriptState_) return;
  const std::string text = padsScriptEdit_->toPlainText().toStdString();
  padsScript_ = sb::input::parseInputScript(text);
  const std::string summary = sb::ui::scriptSummaryLine(padsScript_);
  QString out = QString::fromStdString(summary);
  for (const auto& line : sb::ui::scriptDetailLines(padsScript_, 8))
    out += "\n  " + QString::fromStdString(line);
  padsScriptSummary_->setText(out);
  padsScriptSummary_->setToolTip(out);
  const double t = static_cast<double>(tick_);
  const std::string st =
      sb::ui::scriptStateLine(sb::input::playInputScript(padsScript_, t));
  const QString state =
      QString::fromStdString(st + " (DEMO at t=" + std::to_string(tick_) +
                             " s; no device, no presenter)");
  padsScriptState_->setText(state);
  padsScriptState_->setToolTip(state);
  counters_.add("input.script-parses", 1);
  pushTrace("input", summary);
  logLine("INFO", "input", out.left(300));
}

void UiShell::onRefreshCacheStats() {
  if (!cacheStats_ && !shaderTally_) return;
  const auto loaded = sb::loadFile("scarlettbridge-settings.json");
  const sb::Settings s =
      loaded.ok() ? loaded.value().settings : sb::defaults();
  if (cacheStats_) {
    if (!s.shaderCache) {
      cacheStats_->setText("Shader cache disabled in settings (fingerprint still identifies the translator).");
      logLine("INFO", "shader", "cache stats: disabled in settings");
    } else {
      const auto stats = sb::ui::queryCacheStats(
          s.cachePath, sb::ui::kDefaultCacheMaxEntries,
          static_cast<std::size_t>(s.cacheMB > 0 ? s.cacheMB : 0) * 1024u * 1024u);
      const QString line = QString::fromStdString(sb::ui::cacheStatsLine(stats));
      cacheStats_->setText(line);
      cacheStats_->setToolTip(line);
      counters_.add("shader.stats-refreshes", 1);
      pushTrace("shader", sb::ui::cacheStatsLine(stats));
      logLine("INFO", "shader", line.left(300));
    }
  }
  if (shaderTally_) {
    QString out;
    for (const auto& line :
         sb::ui::failTallyReportLines(shaderFailures_, 8))
      out += QString::fromStdString(line) + "\n";
    if (shaderFailures_.total() == 0)
      out += "no shader batch failures recorded yet (no batch translator run)";
    out = out.trimmed();
    shaderTally_->setText(out);
    shaderTally_->setToolTip(out);
  }
}

void UiShell::onRunBenchmark() {
  if (!benchResult_) return;
  auto noSample = [this](const QString& why) {
    if (benchRate_) benchRate_->setText("No frame-rate sample (" + why + ").");
  };
  auto backend = sb::gfx::d3d12Backend();
  if (!backend.ok()) {
    benchResult_->setText(QString("SKIP (exit 77 equivalent): ") +
                          QString::fromStdString(backend.error().message));
    noSample("smoke skipped");
    logLine("WARN", "benchmark", benchResult_->text());
    return;
  }
  auto adapters = backend.value()->enumerateAdapters(false);
  if (!adapters.ok() || adapters.value().empty()) {
    benchResult_->setText("SKIP: no adapters reported.");
    noSample("smoke skipped");
    logLine("WARN", "benchmark", benchResult_->text());
    return;
  }
  int pick = 0;
  for (std::size_t i = 0; i < adapters.value().size(); ++i)
    if (!adapters.value()[i].isSoftware) {
      pick = static_cast<int>(i);
      break;
    }
  if (auto d = backend.value()->createDevice(pick, false, false); !d.ok()) {
    benchResult_->setText(QString("Device SKIP: ") + QString::fromStdString(d.error().message));
    noSample("smoke skipped");
    logLine("WARN", "benchmark", benchResult_->text());
    return;
  }
  auto smoke = backend.value()->smokeTriangle();
  backend.value()->destroyDevice();
  if (!smoke.ok()) {
    benchResult_->setText(QString("Smoke FAILED: ") +
                          QString::fromStdString(smoke.error().message));
    noSample("smoke failed");
    logLine("ERROR", "benchmark", benchResult_->text());
    return;
  }
  const auto& s = smoke.value();
  benchResult_->setText(
      QString("device ok | compile %1 ms draw %2 ms pixel %3 expected=%4 ticks=%5 (%6 Hz) removed=%7")
          .arg(s.compileMs)
          .arg(s.drawMs)
          .arg(QString::fromStdString(s.pixelHash))
          .arg(s.pixelAsExpected ? "yes" : "no")
          .arg(QString::number(s.timestampTicks))
          .arg(QString::number(s.timestampFreq))
          .arg(QString::fromStdString(backend.value()->deviceRemovedReason())));
  const std::string rate = sb::ui::smokeRateLine(s.drawMs);
  const std::string neck = sb::ui::smokeBottleneckLine();
  if (benchRate_) {
    benchRate_->setText(QString::fromStdString(rate + "\n" + neck));
    benchRate_->setToolTip(QString::fromStdString(rate));
  }
  counters_.add("benchmark.runs", 1);
  pushTrace("benchmark", "smoke run: " + rate);
  logLine("INFO", "benchmark", benchResult_->text());
}

void UiShell::onClearDiagnostics() {
  refreshDiagnostics();
  logLine("INFO", "diagnostics", "view refreshed (ring history kept)");
}

void UiShell::onDiagFilterChanged(const QString&) { refreshDiagnostics(); }

void UiShell::onEngineProbeChanged(const QString& text) {
  if (!engineProbeResult_) return;
  const std::string line = sb::ui::engineInstanceLine(text.toStdString());
  engineProbeResult_->setText(QString::fromStdString(line));
  engineProbeResult_->setToolTip(QString::fromStdString(line));
}

void UiShell::onExportDiagnostics() {
  const std::string filter =
      diagFilterEdit_ ? diagFilterEdit_->text().toStdString() : std::string();
  auto counters = counters_.snapshot();
  auto ev = trace_.snapshot();
  std::vector<sb::diag::TraceEvent> kept;
  for (const auto& e : ev) {
    const std::string hay = e.subsystem + " " + e.message;
    if (sb::ui::matchesFilter(hay, filter)) kept.push_back(e);
  }
  const std::string json = sb::ui::diagnosticsJson(counters, kept);
  const QString path = QFileDialog::getSaveFileName(this, "Export diagnostics JSON", "",
                                                    "JSON (*.json)");
  if (path.isEmpty()) return;
  auto r = sb::ui::exportTextOutsideRepo(path.toStdString(), json);
  if (diagStatus_)
    diagStatus_->setText(r.ok() ? ("Exported " + QString::number(kept.size()) +
                                   " trace events to " + path + ".")
                                : ("Export FAILED: " +
                                   QString::fromStdString(r.error().message)));
  if (r.ok()) {
    counters_.add("diagnostics.exports", 1);
    logLine("INFO", "diagnostics", diagStatus_->text().left(300));
  } else {
    logLine("WARN", "diagnostics", diagStatus_->text().left(300));
  }
}

void UiShell::onLogFilterChanged(const QString&) { refreshLogs(); }

void UiShell::onExportLogs() {
  const std::string filter =
      logFilterEdit_ ? logFilterEdit_->text().toStdString() : std::string();
  std::string out;
  for (const auto& line : logLines_) {
    if (!sb::ui::matchesFilter(line, filter)) continue;
    out += sb::ui::redactLine(line) + "\n";
  }
  const QString path = QFileDialog::getSaveFileName(this, "Export logs (redacted)", "",
                                                    "Text (*.txt *.log)");
  if (path.isEmpty()) return;
  auto r = sb::ui::exportTextOutsideRepo(path.toStdString(), out);
  if (logStatus_)
    logStatus_->setText(r.ok() ? ("Exported to " + path + ". Paths redacted.")
                               : ("Export FAILED: " +
                                  QString::fromStdString(r.error().message)));
  logLine("INFO", "logs", logStatus_ ? logStatus_->text().left(300) : "exported");
}

void UiShell::onClearLogs() {
  refreshLogs();
  logLine("INFO", "logs", "view refreshed (history kept)");
}

void UiShell::logLine(const QString& sev, const QString& sys, const QString& msg) {
  const std::string line =
      QString("[%1 %2/%3] %4").arg(isoNow()).arg(sev, sys, msg).toStdString();
  logLines_.push_back(line);
  if (logLines_.size() > 2000) logLines_.erase(logLines_.begin());
  lastOut_.push(QString("[%1/%2] %3").arg(sev, sys, msg).toStdString());
  refreshLogs();
}

void UiShell::pushTrace(const std::string& sys, const std::string& msg) {
  sb::diag::TraceEvent e;
  e.t = isoNow().toStdString();
  e.subsystem = sys;
  e.message = msg;
  trace_.push(std::move(e));
}

void UiShell::refreshDiagnostics() {
  if (!diagView_) return;
  const std::string filter =
      diagFilterEdit_ ? diagFilterEdit_->text().toStdString() : std::string();
  QString out;
  out += "refusals (translated draws; synthetic ticker binds none):\n";
  out += QString("  %1\n").arg(QString::fromStdString(sb::ui::nullCensusLine(nullCensus_)));
  for (const auto& line :
       sb::ui::perReasonDetailLines(refusalCensus_, sb::diag::PerReasonCensus::kDetailBudget)) {
    if (!sb::ui::matchesFilter(line, filter)) continue;
    out += QString("  %1\n").arg(QString::fromStdString(line));
  }
  if (nullCensus_.total() == 0 && refusalCensus_.total() == 0)
    out += "  no translated draws refused yet\n";
  out += "counters:\n";
  for (const auto& kv : counters_.snapshot()) {
    if (!sb::ui::matchesFilter(kv.first, filter)) continue;
    out += QString("  %1 = %2\n").arg(QString::fromStdString(kv.first)).arg(kv.second);
  }
  out += "recent trace:\n";
  auto ev = trace_.snapshot();
  const std::size_t from = ev.size() > 50 ? ev.size() - 50 : 0;
  for (std::size_t i = from; i < ev.size(); ++i) {
    const std::string hay = ev[i].subsystem + " " + ev[i].message;
    if (!sb::ui::matchesFilter(hay, filter)) continue;
    out += QString("  [%1 %2] %3\n").arg(QString::fromStdString(ev[i].t),
                                         QString::fromStdString(ev[i].subsystem),
                                         QString::fromStdString(ev[i].message));
  }
  diagView_->setPlainText(out);
}

void UiShell::refreshLogs() {
  if (!logView_) return;
  const std::string filter =
      logFilterEdit_ ? logFilterEdit_->text().toStdString() : std::string();
  QString out;
  const std::size_t from = logLines_.size() > 500 ? logLines_.size() - 500 : 0;
  for (std::size_t i = from; i < logLines_.size(); ++i) {
    if (!sb::ui::matchesFilter(logLines_[i], filter)) continue;
    out += QString::fromStdString(logLines_[i]) + "\n";
  }
  logView_->setPlainText(out);
}
