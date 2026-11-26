#include "graphics/d3d12_backend.h"

#if defined(_WIN32) && defined(SB_HAS_D3D12)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include "core/hash.h"
#include "core/logging.h"

namespace sb {
namespace gfx {
namespace {
using Microsoft::WRL::ComPtr;

// Compile with: dxgi.lib d3d12.lib d3dcompiler.lib
const char kVsSrc[] =
    "struct VOut { float4 pos : SV_Position; float4 col : COLOR0; };\n"
    "VOut main(uint vid : SV_VertexID) {\n"
    "  float2 p[3] = { float2(-1,-1), float2(3,-1), float2(-1,3) };\n"
    "  VOut o; o.pos = float4(p[vid], 0, 1); o.col = float4(1.0, 0.25, 0.1, 1.0); return o; }\n";
const char kPsSrc[] =
    "float4 main(float4 pos : SV_Position, float4 col : COLOR0) : SV_Target { return col; }\n";

Result<ComPtr<ID3DBlob>> compile(const char* src, const char* target, const char* entry,
                                 bool debug, double& msOut) {
  UINT flags = D3DCOMPILE_WARNINGS_ARE_ERRORS;
  if (debug) flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
  auto t0 = std::chrono::steady_clock::now();
  ComPtr<ID3DBlob> code, errors;
  HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, target,
                          flags, 0, &code, &errors);
  auto t1 = std::chrono::steady_clock::now();
  msOut = std::chrono::duration<double, std::milli>(t1 - t0).count();
  if (FAILED(hr)) {
    std::string log = errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()),
                                           errors->GetBufferSize())
                             : "unknown";
    return Result<ComPtr<ID3DBlob>>::fail(
        Error::make("graphics/d3d12", 10, "shader compile failed (" + std::string(target) +
                                            "): " + log));
  }
  return Result<ComPtr<ID3DBlob>>::ok(code);
}
}  // namespace

Result<IGraphicsBackend*> d3d12Backend() {
  static D3D12Backend instance;
  return Result<IGraphicsBackend*>::ok(&instance);
}

Result<std::vector<AdapterInfo>> D3D12Backend::enumerateAdapters(bool includeSoftware) {
  ComPtr<IDXGIFactory6> factory;
  UINT dxgiFlags = 0;
#if !defined(NDEBUG)
  dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
  if (FAILED(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory))))
    return Result<std::vector<AdapterInfo>>::fail(
        Error::make("graphics/d3d12", 1, "CreateDXGIFactory2 failed"));
  std::vector<AdapterInfo> out;
  for (UINT i = 0;; ++i) {
    ComPtr<IDXGIAdapter1> a;
    if (factory->EnumAdapters1(i, &a) == DXGI_ERROR_NOT_FOUND) break;
    DXGI_ADAPTER_DESC1 desc{};
    if (FAILED(a->GetDesc1(&desc))) continue;
    if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && !includeSoftware) continue;
    AdapterInfo info;
    char tmp[128];
    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, tmp, sizeof(tmp), nullptr, nullptr);
    info.description = tmp;
    info.vendorId = desc.VendorId;
    info.deviceId = desc.DeviceId;
    info.dedicatedMB = desc.DedicatedVideoMemory / (1024 * 1024);
    info.isSoftware = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
    LARGE_INTEGER umd{};
    if (SUCCEEDED(a->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd)))
      info.umdVersion = std::to_string(umd.HighPart) + "." + std::to_string(umd.LowPart);
    info.supportsD3D12 = SUCCEEDED(
        D3D12CreateDevice(a.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr));
    out.push_back(std::move(info));
  }
  if (out.empty())
    return Result<std::vector<AdapterInfo>>::fail(
        Error::make("graphics/d3d12", 2, "no DXGI adapters found"));
  return Result<std::vector<AdapterInfo>>::ok(std::move(out));
}

// Device + smoke-test objects live for the process; teardown is explicit.
namespace {
ComPtr<ID3D12Device> g_device;
ComPtr<ID3D12CommandQueue> g_queue;
ComPtr<ID3D12CommandAllocator> g_alloc;
ComPtr<ID3D12GraphicsCommandList> g_list;
ComPtr<ID3D12Fence> g_fence;
HANDLE g_fenceEvent = nullptr;
UINT64 g_fenceValue = 0;
}  // namespace

Result<void> D3D12Backend::createDevice(int adapterIndex, bool debugLayer, bool dred) {
  destroyDevice();
  if (debugLayer) {
    ComPtr<ID3D12Debug> dbg;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) dbg->EnableDebugLayer();
  }
  ComPtr<IDXGIFactory6> factory;
  if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
    return Result<void>::fail(Error::make("graphics/d3d12", 1, "CreateDXGIFactory2 failed"));
  ComPtr<IDXGIAdapter1> adapter;
  if (FAILED(factory->EnumAdapters1(static_cast<UINT>(adapterIndex), &adapter)))
    return Result<void>::fail(Error::make("graphics/d3d12", 3, "adapter index out of range"));
  if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device))))
    return Result<void>::fail(Error::make("graphics/d3d12", 4, "D3D12CreateDevice failed"));
  if (debugLayer) {
    ComPtr<ID3D12InfoQueue> iq;
    if (SUCCEEDED(g_device.As(&iq))) {
      iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
      iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    }
  }
  if (dred) {
    ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dredSettings;
    if (SUCCEEDED(g_device.As(&dredSettings))) {
      dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
      dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    } else {
      log::warn("graphics", "DRED requested but not supported by this device/SDK");
    }
  }
  D3D12_COMMAND_QUEUE_DESC qd{};
  qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  if (FAILED(g_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&g_queue))) ||
      FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              IID_PPV_ARGS(&g_alloc))) ||
      FAILED(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_alloc.Get(),
                                         nullptr, IID_PPV_ARGS(&g_list))) ||
      FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence))))
    return Result<void>::fail(Error::make("graphics/d3d12", 5, "queue/allocator/list/fence failed"));
  g_list->Close();
  g_fenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
  if (!g_fenceEvent)
    return Result<void>::fail(Error::make("graphics/d3d12", 6, "fence event failed"));
  deviceReady_ = true;
  return Result<void>::ok();
}

Result<SmokeReport> D3D12Backend::smokeTriangle() {
  SmokeReport rep;
  if (!deviceReady_)
    return Result<SmokeReport>::fail(Error::make("graphics/d3d12", 7, "no device (call createDevice)"));
  double vsMs = 0, psMs = 0;
  auto vs = compile(kVsSrc, "vs_5_0", "main", false, vsMs);
  if (!vs.ok()) return Result<SmokeReport>::fail(vs.error());
  auto ps = compile(kPsSrc, "ps_5_0", "main", false, psMs);
  if (!ps.ok()) return Result<SmokeReport>::fail(ps.error());
  rep.compileMs = vsMs + psMs;
  rep.deviceCreated = true;

  const UINT W = 128, H = 128;
  // Render target.
  D3D12_RESOURCE_DESC rtd{};
  rtd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  rtd.Width = W;
  rtd.Height = H;
  rtd.DepthOrArraySize = 1;
  rtd.MipLevels = 1;
  rtd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  rtd.SampleDesc.Count = 1;
  rtd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  D3D12_HEAP_PROPERTIES heapDefault{};
  heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;
  ComPtr<ID3D12Resource> rt;
  if (FAILED(g_device->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &rtd,
                                              D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr,
                                              IID_PPV_ARGS(&rt))))
    return Result<SmokeReport>::fail(Error::make("graphics/d3d12", 8, "render target alloc failed"));
  // RTV heap (CPU-visible).
  D3D12_DESCRIPTOR_HEAP_DESC hd{};
  hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  hd.NumDescriptors = 1;
  ComPtr<ID3D12DescriptorHeap> rtvHeap;
  if (FAILED(g_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap))))
    return Result<SmokeReport>::fail(Error::make("graphics/d3d12", 9, "RTV heap failed"));
  g_device->CreateRenderTargetView(rt.Get(), nullptr, rtvHeap->GetCPUDescriptorHandleForHeapStart());
  // Root signature (empty).
  D3D12_ROOT_SIGNATURE_DESC rsd{};
  rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  ComPtr<ID3DBlob> sig, sigErr;
  if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &sigErr)))
    return Result<SmokeReport>::fail(Error::make("graphics/d3d12", 10, "root signature failed"));
  ComPtr<ID3D12RootSignature> rootSig;
  if (FAILED(g_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
                                          IID_PPV_ARGS(&rootSig))))
    return Result<SmokeReport>::fail(Error::make("graphics/d3d12", 10, "root signature failed"));
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psd{};
  psd.pRootSignature = rootSig.Get();
  psd.VS = {vs.value()->GetBufferPointer(), vs.value()->GetBufferSize()};
  psd.PS = {ps.value()->GetBufferPointer(), ps.value()->GetBufferSize()};
  psd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  psd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  psd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  psd.SampleMask = UINT_MAX;
  psd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psd.NumRenderTargets = 1;
  psd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  psd.SampleDesc.Count = 1;
  ComPtr<ID3D12PipelineState> pso;
  if (FAILED(g_device->CreateGraphicsPipelineState(&psd, IID_PPV_ARGS(&pso))))
    return Result<SmokeReport>::fail(Error::make("graphics/d3d12", 11, "PSO creation failed"));

  auto t0 = std::chrono::steady_clock::now();
  g_alloc->Reset();
  g_list->Reset(g_alloc.Get(), pso.Get());
  const float clear[4] = {0, 0, 0, 1};
  auto rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  g_list->ClearRenderTargetView(rtv, clear, 0, nullptr);
  g_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  D3D12_VIEWPORT vp{0, 0, static_cast<float>(W), static_cast<float>(H), 0, 1};
  D3D12_RECT sc{0, 0, static_cast<LONG>(W), static_cast<LONG>(H)};
  g_list->RSSetViewports(1, &vp);
  g_list->RSSetScissorRects(1, &sc);
  g_list->SetGraphicsRootSignature(rootSig.Get());
  g_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  if (timestamps) {
    UINT64 freq = 0;
    if (SUCCEEDED(g_queue->GetTimestampFrequency(&freq))) rep.timestampFreq = freq;
    g_list->EndQuery(qheap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
  }
  g_list->DrawInstanced(3, 1, 0, 0);
  if (timestamps) {
    g_list->EndQuery(qheap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
    g_list->ResolveQueryData(qheap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, readback.Get(),
                             W * H * 4);
  }
  // Copy to readback.
  D3D12_HEAP_PROPERTIES heapReadback{};
  heapReadback.Type = D3D12_HEAP_TYPE_READBACK;
  D3D12_RESOURCE_DESC bd{};
  bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bd.Width = W * H * 4 + 16;  // pixels + two UINT64 GPU timestamps
  bd.Height = 1;
  bd.DepthOrArraySize = 1;
  bd.MipLevels = 1;
  bd.SampleDesc.Count = 1;
  bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  ComPtr<ID3D12Resource> readback;
  if (FAILED(g_device->CreateCommittedResource(&heapReadback, D3D12_HEAP_FLAG_NONE, &bd,
                                              D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                              IID_PPV_ARGS(&readback))))
    return Result<SmokeReport>::fail(Error::make("graphics/d3d12", 12, "readback alloc failed"));
  // Timestamp queries: one marker each side of the draw.
  ComPtr<ID3D12QueryHeap> qheap;
  bool timestamps = false;
  {
    D3D12_QUERY_HEAP_DESC qd{};
    qd.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    qd.Count = 2;
    timestamps = SUCCEEDED(g_device->CreateQueryHeap(&qd, IID_PPV_ARGS(&qheap)));
    if (!timestamps) rep.warnings.push_back("timestamp query heap unavailable");
  }
  D3D12_TEXTURE_COPY_LOCATION srcLoc{rt.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
  D3D12_TEXTURE_COPY_LOCATION dstLoc{readback.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {}};
  dstLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  dstLoc.PlacedFootprint.Footprint.Width = W;
  dstLoc.PlacedFootprint.Footprint.Height = H;
  dstLoc.PlacedFootprint.Footprint.Depth = 1;
  dstLoc.PlacedFootprint.Footprint.RowPitch = W * 4;
  g_list->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
  g_list->Close();
  ID3D12CommandList* lists[] = {g_list.Get()};
  g_queue->ExecuteCommandLists(1, lists);
  ++g_fenceValue;
  g_queue->Signal(g_fence.Get(), g_fenceValue);
  if (g_fence->GetCompletedValue() < g_fenceValue)
    WaitForSingleObject(g_fenceEvent, 5000);
  auto t1 = std::chrono::steady_clock::now();
  rep.drawMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

  void* mapped = nullptr;
  D3D12_RANGE range{0, W * H * 4 + 16};
  if (FAILED(readback->Map(0, &range, &mapped)))
    return Result<SmokeReport>::fail(Error::make("graphics/d3d12", 13, "readback map failed"));
  const auto* px = static_cast<const std::uint8_t*>(mapped);
  std::size_t center = (64 * W + 64) * 4;
  for (int i = 0; i < 4; ++i) rep.centerPixel[i] = px[center + i];
  readback->Unmap(0, nullptr);
  if (timestamps) {
    std::uint64_t t0 = 0, t1 = 0;
    std::memcpy(&t0, px + W * H * 4, 8);
    std::memcpy(&t1, px + W * H * 4 + 8, 8);
    if (t1 >= t0 && (t0 != 0 || t1 != 0)) {
      rep.timestampTicks = t1 - t0;
      rep.timestampResolved = true;
    } else {
      rep.warnings.push_back("timestamp resolve returned zeros (adapter may not support it)");
    }
  }
  rep.pixelHash = fnv1a32hex(rep.centerPixel, 4);
  // Expected solid color (1.0, 0.25, 0.1, 1.0) -> (255, ~64, ~25, 255), ±1 rounding.
  rep.pixelAsExpected =
      rep.centerPixel[0] == 255 && rep.centerPixel[3] == 255 &&
      (rep.centerPixel[1] >= 63 && rep.centerPixel[1] <= 65) &&
      (rep.centerPixel[2] >= 24 && rep.centerPixel[2] <= 26);
  std::string removed = deviceRemovedReason();
  if (removed != "S_OK") {
    rep.warnings.push_back("device removed: " + removed);
    log::warn("graphics", "D3D12 device removed: " + removed);
  }
  return Result<SmokeReport>::ok(rep);
}

Result<void> D3D12Backend::createSwapchain(int, int, bool) {
  return Result<void>::fail(
      Error::make("graphics/d3d12", 30, "Unsupported: presentation path has no synthetic tests yet"));
}

Result<void> D3D12Backend::present() {
  return Result<void>::fail(
      Error::make("graphics/d3d12", 31, "Unsupported: presentation path has no synthetic tests yet"));
}

std::string D3D12Backend::deviceRemovedReason() {
  if (!g_device) return "no-device";
  HRESULT hr = g_device->GetDeviceRemovedReason();
  if (hr == S_OK) return "S_OK";
  char buf[16];
  std::snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(hr));
  return std::string(buf);
}

void D3D12Backend::destroyDevice() {  if (g_fenceEvent) {
    CloseHandle(g_fenceEvent);
    g_fenceEvent = nullptr;
  }
  g_list.Reset();
  g_alloc.Reset();
  g_queue.Reset();
  g_fence.Reset();
  g_device.Reset();
  deviceReady_ = false;
}

}  // namespace gfx
}  // namespace sb

#else
// Non-Windows: explicit Unsupported (the unit-test suite never touches this).
#include "graphics/d3d12_backend.h"
namespace sb {
namespace gfx {
Result<IGraphicsBackend*> d3d12Backend() {
  return Result<IGraphicsBackend*>::fail(
      Error::make("graphics", 1, "D3D12 backend is Windows-only"));
}
}  // namespace gfx
}  // namespace sb
#endif
