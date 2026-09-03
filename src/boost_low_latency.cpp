// Boost: Low-Latency Present  (+ GPU thread priority)
// ------------------------------------------------------------------------
// Two tweaks against the same IDXGIDevice1 we get from the ANGLE D3D11
// device, gated by separate config flags:
//
//   1. low_latency=true  → SetMaximumFrameLatency(1) - DXGI default is 3
//      queued frames; cutting to 1 saves ~33 ms input lag at 60 FPS.
//
//   2. gpu_thread_prio=true → SetGPUThreadPriority(+7) - bumps the user-
//      mode driver's command-list submission thread priority. Default is
//      0; range is [-7..+7]. Helps our GPU work win against other GPU
//      clients (DWM compositor, browser GPU process). On modern Win10
//      this works without admin; older versions silently clamp non-zero
//      values when not elevated, which is harmless.

#include "angle_loader.hpp"
#include "config.hpp"
#include <windows.h>


// IDXGIDevice1 GUID
static const GUID IID_IDXGIDevice1 = {
    0x77db970f,
    0x6276,
    0x48ba,
    {0xba, 0x28, 0x07, 0x01, 0x43, 0xb4, 0x39, 0x2c}};

namespace boost_low_latency {

void apply() {
  const auto &cfg = Config::get();
  // Either feature can request this module; skip if both off.
  if (!cfg.low_latency && !cfg.gpu_thread_prio)
    return;

  // Gate by backend - eglQueryDeviceAttribEXT(EGL_D3D11_DEVICE_ANGLE) is
  // only meaningful on the D3D11 backend. On D3D9 the call may load
  // d3d11.dll into the process or return a stale/uninitialized pointer
  // that crashes when QI'd.
  const std::string &be = cfg.backend;
  if (be != "d3d11") {
    angle::log("low_latency: backend=%s (not d3d11), skipping", be.c_str());
    return;
  }

  auto &a = angle::state();
  if (!a.egl || !a.display) {
    angle::log("low_latency: ANGLE not ready");
    return;
  }

  // Get D3D11 device from ANGLE
  using QueryDisplayAttribFn = int (*)(void *, int, intptr_t *);
  using QueryDeviceAttribFn = int (*)(void *, int, intptr_t *);

  auto queryDisplay =
      (QueryDisplayAttribFn)GetProcAddress(a.egl, "eglQueryDisplayAttribEXT");
  auto queryDevice =
      (QueryDeviceAttribFn)GetProcAddress(a.egl, "eglQueryDeviceAttribEXT");

  if (!queryDisplay || !queryDevice) {
    angle::log("low_latency: EGL extensions not available");
    return;
  }

  intptr_t eglDevice = 0;
  if (!queryDisplay(a.display, 0x322C, &eglDevice) || !eglDevice)
    return;

  intptr_t d3d11Dev = 0;
  if (!queryDevice((void *)eglDevice, 0x33A1, &d3d11Dev) || !d3d11Dev)
    return;

  // QI for IDXGIDevice1
  struct VtblBase {
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(void *, const GUID &, void **);
    ULONG(STDMETHODCALLTYPE *AddRef)(void *);
    ULONG(STDMETHODCALLTYPE *Release)(void *);
  };

  auto *vtbl = *(VtblBase **)d3d11Dev;
  void *dxgiDevice = nullptr;
  HRESULT hr =
      vtbl->QueryInterface((void *)d3d11Dev, IID_IDXGIDevice1, &dxgiDevice);

  if (SUCCEEDED(hr) && dxgiDevice) {
    // IDXGIDevice1 vtable (verified against dxgi.h):
    //   0: QueryInterface
    //   1: AddRef
    //   2: Release
    //   --- IDXGIObject (4 methods) ---
    //   3: SetPrivateData
    //   4: SetPrivateDataInterface
    //   5: GetPrivateData
    //   6: GetParent
    //   --- IDXGIDevice (4 methods) ---
    //   7: GetAdapter
    //   8: CreateSurface
    //   9: QueryResourceResidency
    //  10: SetGPUThreadPriority   <-- INT priority [-7..+7]
    //  11: GetGPUThreadPriority   <-- previously WRONGLY used as Set
    //   --- IDXGIDevice1 (2 methods) ---
    //  12: SetMaximumFrameLatency <-- correct slot
    //  13: GetMaximumFrameLatency
    //
    // The old code called slot 11 (GetGPUThreadPriority(INT* pPriority))
    // with `1` reinterpreted as a pointer, producing the
    // "Failed to write to memory at 0x1" crash inside d3d11.dll.
    using SetGpuPrioFn = HRESULT(STDMETHODCALLTYPE *)(void *, INT);
    using SetMaxLatencyFn = HRESULT(STDMETHODCALLTYPE *)(void *, UINT);
    auto **vt = *(void ***)dxgiDevice;

    // Tweak 2: GPU command-list submission thread priority.
    if (cfg.gpu_thread_prio) {
      auto setGpuPrio = (SetGpuPrioFn)vt[10];
      hr = setGpuPrio(dxgiDevice, +7);
      if (SUCCEEDED(hr)) {
        angle::log("gpu_thread_prio: GPU thread priority = +7 (max)");
      } else {
        // Try +5 then +2 in case +7 is rejected without admin.
        hr = setGpuPrio(dxgiDevice, +5);
        if (SUCCEEDED(hr)) {
          angle::log("gpu_thread_prio: GPU thread priority = +5 (clamped)");
        } else {
          hr = setGpuPrio(dxgiDevice, +2);
          if (SUCCEEDED(hr)) {
            angle::log("gpu_thread_prio: GPU thread priority = +2 (clamped)");
          } else {
            angle::log("gpu_thread_prio: SetGPUThreadPriority failed (0x%lx)",
                       hr);
          }
        }
      }
    }

    // Tweak 1: max queued frames.
    if (cfg.low_latency) {
      auto setMaxLatency = (SetMaxLatencyFn)vt[12];
      hr = setMaxLatency(dxgiDevice, 1);
      if (SUCCEEDED(hr)) {
        angle::log("low_latency: MaxFrameLatency=1 (was 3)");
      } else {
        angle::log("low_latency: SetMaximumFrameLatency failed (0x%lx)", hr);
      }
    }

    auto *vtbl2 = *(VtblBase **)dxgiDevice;
    vtbl2->Release(dxgiDevice);
  } else {
    angle::log("low_latency: IDXGIDevice1 not available");
  }
}
} // namespace boost_low_latency
