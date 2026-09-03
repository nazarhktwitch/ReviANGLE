// Boost: DXGI Allow Tearing - full D3D11 FPS bypass.
//
// Pipeline:
//   1. IAT-hook libGLESv2.dll's import of CreateDXGIFactory1/2 -> our wrapper.
//   2. Wrapper calls real, then patches the returned IDXGIFactory*'s vtable:
//        slot 10 (IDXGIFactory::CreateSwapChain),
//        slot 15 (IDXGIFactory2::CreateSwapChainForHwnd),
//        slot 16 (IDXGIFactory2::CreateSwapChainForCoreWindow),
//        slot 24 (IDXGIFactory2::CreateSwapChainForComposition).
//      Vtable lives in DXGI.dll's RO data - we VirtualProtect it RW first.
//   3. Our CreateSwapChain* injects DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING into
//      desc->Flags before forwarding, then patches the returned swap chain's
//      vtable: slot 8 (Present) and slot 22 (Present1 on IDXGISwapChain1).
//   4. Our Present(SyncInterval, Flags): if SyncInterval==0, OR Flags with
//      DXGI_PRESENT_ALLOW_TEARING -> driver presents instantly, no VBlank wait.
//
// Result: FPS unlocked above monitor refresh on D3D11 backend, matching what
// the FPS Bypass mod accomplishes on D3D9. Toggle via [BoostLatency]
// allow_tearing.

#include "boost_allow_tearing.hpp"
#include "angle_loader.hpp"
#include "common/iat_hook.hpp"
#include "config.hpp"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <unknwn.h> // IUnknown / REFIID - needed for COM call signatures
#include <windows.h>


// --- DXGI constants ---------------------------------------------------------
#ifndef DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
#define DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING 2048
#endif
#ifndef DXGI_PRESENT_ALLOW_TEARING
#define DXGI_PRESENT_ALLOW_TEARING 0x00000200UL
#endif
#ifndef DXGI_FEATURE_PRESENT_ALLOW_TEARING
#define DXGI_FEATURE_PRESENT_ALLOW_TEARING 0
#endif

// --- DXGI struct shapes (only the bits we touch) ---------------------------
// We avoid #include <dxgi.h>/<dxgi1_5.h> to stay header-clean, but the layouts
// here MUST match Windows SDK exactly. Verified against MSDN x64 layout.
//
// DXGI_MODE_DESC is 28 bytes:
//   UINT Width(4) + UINT Height(4) + DXGI_RATIONAL RefreshRate(8) +
//   DXGI_FORMAT Format(4) + DXGI_MODE_SCANLINE_ORDER(4) + DXGI_MODE_SCALING(4)
// Previous version used [44] which writes flags at offset 80 instead of 64,
// producing 16 B of heap/stack corruption past the real DXGI_SWAP_CHAIN_DESC
// (68 B) - and crashed d3d11.dll inside CreateSwapChain at +0xf2f5.
struct DXGI_SWAP_CHAIN_DESC_min {
  BYTE bufferDesc[28]; // DXGI_MODE_DESC (28 B, was incorrectly 44)
  BYTE sampleDesc[8];  // DXGI_SAMPLE_DESC
  UINT bufferUsage;    // offset 36
  UINT bufferCount;    // offset 40
  HWND outputWindow;   // offset 48 (4 B compiler padding for 8-B align on x64)
  BOOL windowed;       // offset 56
  UINT swapEffect;     // offset 60
  UINT flags;          // offset 64 - the field we modify
};
struct DXGI_SWAP_CHAIN_DESC1_min {
  UINT width;
  UINT height;
  UINT format;
  BOOL stereo;
  BYTE sampleDesc[8];
  UINT bufferUsage;
  UINT bufferCount;
  UINT scaling;
  UINT swapEffect;
  UINT alphaMode;
  UINT flags; // <-- the field we modify
};

// --- DXGI GUIDs ------------------------------------------------------------
static const GUID IID_IDXGIFactory5 = {
    0x7632e1f5,
    0xee65,
    0x4dca,
    {0x87, 0xfd, 0x84, 0xcd, 0x75, 0xf8, 0x83, 0x8d}};
static const GUID IID_IDXGISwapChain1 = {
    0x790a45f7,
    0x0d42,
    0x4876,
    {0x98, 0x3a, 0x0a, 0x55, 0xcf, 0xe6, 0xf4, 0xaa}};

// --- COM call types ---------------------------------------------------------
using PFN_CreateDXGIFactory1 = HRESULT(WINAPI *)(REFIID, void **);
using PFN_CreateDXGIFactory2 = HRESULT(WINAPI *)(UINT, REFIID, void **);

using PFN_CreateSwapChain = HRESULT(STDMETHODCALLTYPE *)(
    void *, IUnknown *, DXGI_SWAP_CHAIN_DESC_min *, void **);
using PFN_CreateSwapChainForHwnd = HRESULT(STDMETHODCALLTYPE *)(
    void *, IUnknown *, HWND, const DXGI_SWAP_CHAIN_DESC1_min *, const void *,
    void *, void **);
using PFN_CreateSwapChainForCoreWindow = HRESULT(STDMETHODCALLTYPE *)(
    void *, IUnknown *, IUnknown *, const DXGI_SWAP_CHAIN_DESC1_min *, void *,
    void **);
using PFN_CreateSwapChainForComposition = HRESULT(STDMETHODCALLTYPE *)(
    void *, IUnknown *, const DXGI_SWAP_CHAIN_DESC1_min *, void *, void **);

using PFN_Present = HRESULT(STDMETHODCALLTYPE *)(void *, UINT, UINT);
using PFN_Present1 = HRESULT(STDMETHODCALLTYPE *)(void *, UINT, UINT,
                                                  const void *);

// --- Globals ---------------------------------------------------------------
static std::atomic<bool> g_enabled{false};
static std::atomic<bool> g_tearingSupported{false};
static std::atomic<bool> g_factoryVtblPatched{false};
static std::atomic<bool> g_swapVtblPatched{false};
static std::atomic<bool> g_swap1VtblPatched{false};

static PFN_CreateDXGIFactory1 real_CreateDXGIFactory1 = nullptr;
static PFN_CreateDXGIFactory2 real_CreateDXGIFactory2 = nullptr;

static PFN_CreateSwapChain real_CreateSwapChain = nullptr;
static PFN_CreateSwapChainForHwnd real_CreateSwapChainForHwnd = nullptr;
static PFN_CreateSwapChainForCoreWindow real_CreateSwapChainForCoreWindow =
    nullptr;
static PFN_CreateSwapChainForComposition real_CreateSwapChainForComposition =
    nullptr;

static PFN_Present real_Present = nullptr;
static PFN_Present1 real_Present1 = nullptr;

// --- Vtable patch helper ---------------------------------------------------
// In-place patch of `slot` on the given object's vtable (which lives in
// DXGI.dll's RO data, so we VirtualProtect it RW for the duration of the swap,
// then restore). Subsequent objects of the same vtable type inherit the hook
// automatically.
static void *patchVtableSlot(void *obj, int slot, void *newFn) {
  if (!obj || !newFn)
    return nullptr;
  void **vtbl = *(void ***)obj;
  if (!vtbl)
    return nullptr;
  void **slotAddr = &vtbl[slot];

  DWORD old = 0;
  if (!VirtualProtect(slotAddr, sizeof(void *), PAGE_READWRITE, &old)) {
    angle::log("allow_tearing: VirtualProtect RW failed at vtbl[%d] (gle=%lu)",
               slot, GetLastError());
    return nullptr;
  }
  void *original = *slotAddr;
  if (original == newFn) {
    // already patched
    DWORD tmp;
    VirtualProtect(slotAddr, sizeof(void *), old, &tmp);
    return nullptr;
  }
  *slotAddr = newFn;
  DWORD tmp;
  VirtualProtect(slotAddr, sizeof(void *), old, &tmp);
  return original;
}

// --- Forward declarations of swap chain hooks ------------------------------
static void detourSwapChainVtables(void *swap);

// --- Present hooks ---------------------------------------------------------
static HRESULT STDMETHODCALLTYPE Hook_Present(void *self, UINT syncInterval,
                                              UINT flags) {
  // Force unlocked present when caller asks for vsync-off.
  // ANGLE invokes Present(0, 0) when eglSwapInterval(0) is set; without
  // ALLOW_TEARING flag, DXGI flip-model swap chain still waits for vblank.
  if (syncInterval == 0 && g_tearingSupported.load(std::memory_order_relaxed)) {
    flags |= DXGI_PRESENT_ALLOW_TEARING;
  }
  return real_Present(self, syncInterval, flags);
}
static HRESULT STDMETHODCALLTYPE Hook_Present1(void *self, UINT syncInterval,
                                               UINT flags, const void *params) {
  if (syncInterval == 0 && g_tearingSupported.load(std::memory_order_relaxed)) {
    flags |= DXGI_PRESENT_ALLOW_TEARING;
  }
  return real_Present1(self, syncInterval, flags, params);
}

// Patch Present (slot 8) on IDXGISwapChain, and Present1 (slot 22) on
// IDXGISwapChain1.
static void detourSwapChainVtables(void *swap) {
  if (!swap)
    return;

  bool expected = false;
  if (g_swapVtblPatched.compare_exchange_strong(expected, true)) {
    void *origPresent = patchVtableSlot(swap, 8, (void *)&Hook_Present);
    if (origPresent) {
      real_Present = (PFN_Present)origPresent;
      angle::log("allow_tearing: patched IDXGISwapChain::Present (orig=%p)",
                 origPresent);
    } else {
      // Already patched or failed. If real_Present is null, undo the latch.
      if (!real_Present)
        g_swapVtblPatched.store(false);
    }
  }

  // QI for IDXGISwapChain1 to also hook Present1 (used by Composition path).
  expected = false;
  if (g_swap1VtblPatched.compare_exchange_strong(expected, true)) {
    struct IUnknownVtbl {
      HRESULT(STDMETHODCALLTYPE *QueryInterface)(void *, REFIID, void **);
      ULONG(STDMETHODCALLTYPE *AddRef)(void *);
      ULONG(STDMETHODCALLTYPE *Release)(void *);
    };
    auto *vtbl = *(IUnknownVtbl **)swap;
    void *swap1 = nullptr;
    HRESULT hr = vtbl->QueryInterface(swap, IID_IDXGISwapChain1, &swap1);
    if (SUCCEEDED(hr) && swap1) {
      void *origPresent1 = patchVtableSlot(swap1, 22, (void *)&Hook_Present1);
      if (origPresent1) {
        real_Present1 = (PFN_Present1)origPresent1;
        angle::log("allow_tearing: patched IDXGISwapChain1::Present1 (orig=%p)",
                   origPresent1);
      }
      auto *swap1Vtbl = *(IUnknownVtbl **)swap1;
      swap1Vtbl->Release(swap1);
    } else {
      g_swap1VtblPatched.store(false);
    }
  }

  g_enabled.store(true, std::memory_order_release);
}

// --- CreateSwapChain* hooks (each injects ALLOW_TEARING into desc->Flags) --
//
// IMPORTANT: DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING is ONLY valid for flip-model
// swap chains (SwapEffect = FLIP_DISCARD = 4 or FLIP_SEQUENTIAL = 3). On the
// legacy BLIT model (DISCARD = 0, SEQUENTIAL = 1) DXGI returns
// DXGI_ERROR_INVALID_CALL → ANGLE bubbles up EGL_BAD_ALLOC (0x3003) on
// eglCreateWindowSurface, which kills GD's window context creation.
//
// ANGLE's swap-chain backend may pick BLIT model on certain hardware/driver
// combos (e.g. older Intel HD, legacy Nvidia Kepler). We must only inject the
// flag when the requested swap effect is already flip-model; otherwise the
// flag is a no-op anyway and would only break things.
static inline bool isFlipModel(UINT swapEffect) {
  // 3 = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 4 = DXGI_SWAP_EFFECT_FLIP_DISCARD
  return swapEffect == 3 || swapEffect == 4;
}

static HRESULT STDMETHODCALLTYPE Hook_CreateSwapChain(
    void *self, IUnknown *dev, DXGI_SWAP_CHAIN_DESC_min *desc, void **out) {
  bool injected = false;
  if (desc && isFlipModel(desc->swapEffect)) {
    desc->flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    injected = true;
  }
  HRESULT hr = real_CreateSwapChain(self, dev, desc, out);
  if (SUCCEEDED(hr) && out && *out) {
    angle::log("allow_tearing: CreateSwapChain ok, swapEffect=%u flags=0x%X "
               "tearing=%d sc=%p",
               desc ? desc->swapEffect : 0, desc ? desc->flags : 0,
               (int)injected, *out);
    if (injected)
      detourSwapChainVtables(*out);
  } else if (FAILED(hr)) {
    angle::log("allow_tearing: CreateSwapChain FAILED hr=0x%lx swapEffect=%u",
               hr, desc ? desc->swapEffect : 0);
  }
  return hr;
}
static HRESULT STDMETHODCALLTYPE Hook_CreateSwapChainForHwnd(
    void *self, IUnknown *dev, HWND hwnd, const DXGI_SWAP_CHAIN_DESC1_min *desc,
    const void *fsdesc, void *output, void **out) {
  DXGI_SWAP_CHAIN_DESC1_min patched{};
  bool injected = false;
  if (desc) {
    patched = *desc;
    if (isFlipModel(patched.swapEffect)) {
      patched.flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
      injected = true;
    }
  }
  HRESULT hr = real_CreateSwapChainForHwnd(
      self, dev, hwnd, desc ? &patched : nullptr, fsdesc, output, out);
  if (SUCCEEDED(hr) && out && *out) {
    angle::log("allow_tearing: CreateSwapChainForHwnd ok, swapEffect=%u "
               "flags=0x%X tearing=%d sc=%p",
               patched.swapEffect, patched.flags, (int)injected, *out);
    if (injected)
      detourSwapChainVtables(*out);
  } else if (FAILED(hr)) {
    angle::log(
        "allow_tearing: CreateSwapChainForHwnd FAILED hr=0x%lx swapEffect=%u",
        hr, patched.swapEffect);
  }
  return hr;
}
static HRESULT STDMETHODCALLTYPE Hook_CreateSwapChainForCoreWindow(
    void *self, IUnknown *dev, IUnknown *window,
    const DXGI_SWAP_CHAIN_DESC1_min *desc, void *output, void **out) {
  DXGI_SWAP_CHAIN_DESC1_min patched{};
  bool injected = false;
  if (desc) {
    patched = *desc;
    if (isFlipModel(patched.swapEffect)) {
      patched.flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
      injected = true;
    }
  }
  HRESULT hr = real_CreateSwapChainForCoreWindow(
      self, dev, window, desc ? &patched : nullptr, output, out);
  if (SUCCEEDED(hr) && out && *out) {
    angle::log(
        "allow_tearing: CreateSwapChainForCoreWindow ok, tearing=%d sc=%p",
        (int)injected, *out);
    if (injected)
      detourSwapChainVtables(*out);
  }
  return hr;
}
static HRESULT STDMETHODCALLTYPE Hook_CreateSwapChainForComposition(
    void *self, IUnknown *dev, const DXGI_SWAP_CHAIN_DESC1_min *desc,
    void *output, void **out) {
  DXGI_SWAP_CHAIN_DESC1_min patched{};
  bool injected = false;
  if (desc) {
    patched = *desc;
    if (isFlipModel(patched.swapEffect)) {
      patched.flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
      injected = true;
    }
  }
  HRESULT hr = real_CreateSwapChainForComposition(
      self, dev, desc ? &patched : nullptr, output, out);
  if (SUCCEEDED(hr) && out && *out) {
    angle::log(
        "allow_tearing: CreateSwapChainForComposition ok, tearing=%d sc=%p",
        (int)injected, *out);
    if (injected)
      detourSwapChainVtables(*out);
  }
  return hr;
}

// --- Factory vtable patch: install all CreateSwapChain* hooks --------------
static void detourFactoryVtable(void *factory) {
  if (!factory)
    return;

  bool expected = false;
  if (!g_factoryVtblPatched.compare_exchange_strong(expected, true))
    return;

  // IDXGIFactory::CreateSwapChain  (slot 10)
  void *p10 = patchVtableSlot(factory, 10, (void *)&Hook_CreateSwapChain);
  if (p10) {
    real_CreateSwapChain = (PFN_CreateSwapChain)p10;
    angle::log("allow_tearing: patched IDXGIFactory::CreateSwapChain (orig=%p)",
               p10);
  }

  // IDXGIFactory2::CreateSwapChainForHwnd  (slot 15)
  void *p15 =
      patchVtableSlot(factory, 15, (void *)&Hook_CreateSwapChainForHwnd);
  if (p15) {
    real_CreateSwapChainForHwnd = (PFN_CreateSwapChainForHwnd)p15;
    angle::log("allow_tearing: patched IDXGIFactory2::CreateSwapChainForHwnd "
               "(orig=%p)",
               p15);
  }

  // IDXGIFactory2::CreateSwapChainForCoreWindow  (slot 16)
  void *p16 =
      patchVtableSlot(factory, 16, (void *)&Hook_CreateSwapChainForCoreWindow);
  if (p16) {
    real_CreateSwapChainForCoreWindow = (PFN_CreateSwapChainForCoreWindow)p16;
  }

  // IDXGIFactory2::CreateSwapChainForComposition (slot 24)
  void *p24 =
      patchVtableSlot(factory, 24, (void *)&Hook_CreateSwapChainForComposition);
  if (p24) {
    real_CreateSwapChainForComposition = (PFN_CreateSwapChainForComposition)p24;
  }
}

// --- IAT hook entry points: invoke real CreateDXGIFactory*, then detour ---
static HRESULT WINAPI Hook_CreateDXGIFactory1(REFIID iid, void **out) {
  if (!real_CreateDXGIFactory1)
    return E_FAIL;
  HRESULT hr = real_CreateDXGIFactory1(iid, out);
  if (SUCCEEDED(hr) && out && *out) {
    angle::log("allow_tearing: CreateDXGIFactory1 -> %p", *out);
    detourFactoryVtable(*out);
  }
  return hr;
}
static HRESULT WINAPI Hook_CreateDXGIFactory2(UINT flags, REFIID iid,
                                              void **out) {
  if (!real_CreateDXGIFactory2)
    return E_FAIL;
  HRESULT hr = real_CreateDXGIFactory2(flags, iid, out);
  if (SUCCEEDED(hr) && out && *out) {
    angle::log("allow_tearing: CreateDXGIFactory2(0x%X) -> %p", flags, *out);
    detourFactoryVtable(*out);
  }
  return hr;
}

// --- Tearing support probe: test DXGI 1.5 + driver capability --------------
static bool probeTearingSupport() {
  HMODULE dxgi = GetModuleHandleA("dxgi.dll");
  if (!dxgi)
    dxgi = LoadLibraryA("dxgi.dll");
  if (!dxgi) {
    angle::log("allow_tearing: dxgi.dll not present");
    return false;
  }

  using FnFactory1 = HRESULT(WINAPI *)(REFIID, void **);
  auto createFactory1 = (FnFactory1)GetProcAddress(dxgi, "CreateDXGIFactory1");
  if (!createFactory1)
    return false;

  static const GUID IID_IDXGIFactory1 = {
      0x770aae78,
      0xf26f,
      0x4dba,
      {0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87}};

  void *factory = nullptr;
  if (FAILED(createFactory1(IID_IDXGIFactory1, &factory)) || !factory)
    return false;

  struct IUnknownVtbl {
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(void *, REFIID, void **);
    ULONG(STDMETHODCALLTYPE *AddRef)(void *);
    ULONG(STDMETHODCALLTYPE *Release)(void *);
  };
  auto *vtbl = *(IUnknownVtbl **)factory;

  void *factory5 = nullptr;
  bool supported = false;
  if (SUCCEEDED(vtbl->QueryInterface(factory, IID_IDXGIFactory5, &factory5)) &&
      factory5) {
    // IDXGIFactory5::CheckFeatureSupport is at vtable slot 27:
    //   (3 IUnknown) + (4 IDXGIObject) + (5 IDXGIFactory) + (2 IDXGIFactory1)
    //   + (11 IDXGIFactory2) + (1 IDXGIFactory3:GetCreationFlags) + (1
    //   IDXGIFactory4:EnumAdapterByLuid) = (1) IDXGIFactory4:EnumWarpAdapter
    //   ... actually: IDXGIFactory: 12 methods (3+4+5) IDXGIFactory1 adds 2 ->
    //   14 IDXGIFactory2 adds 11 -> 25 IDXGIFactory3 adds 1  -> 26
    //   IDXGIFactory4 adds 2  -> 28
    //   IDXGIFactory5 adds 1 (CheckFeatureSupport) -> slot 28
    using CheckFeatureFn =
        HRESULT(STDMETHODCALLTYPE *)(void *, UINT, void *, UINT);
    auto **vt5 = *(void ***)factory5;
    auto checkFeature = (CheckFeatureFn)vt5[28];
    BOOL allowTearing = FALSE;
    HRESULT hr = checkFeature(factory5, DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                              &allowTearing, sizeof(allowTearing));
    if (SUCCEEDED(hr) && allowTearing) {
      supported = true;
    }
    auto *vtbl5 = *(IUnknownVtbl **)factory5;
    vtbl5->Release(factory5);
  }
  vtbl->Release(factory);

  angle::log("allow_tearing: DXGI feature probe -> %s",
             supported ? "SUPPORTED" : "not supported");
  return supported;
}

namespace boost_allow_tearing {

void apply() {
  if (!Config::get().allow_tearing) {
    angle::log("allow_tearing: disabled by config");
    return;
  }

  // Gate: ALLOW_TEARING is a DXGI flip-model swap chain feature - it only
  // exists when ANGLE uses the D3D11 backend. On D3D9 backend ANGLE
  // presents through IDirect3DSwapChain9 which has no concept of tearing.
  // Worse: probing/hooking DXGI on D3D9 path can pull d3d11.dll into the
  // process and trigger AVs in d3d11.dll (observed empirically as a
  // wglMakeCurrent → d3d11.dll!0xf2f5 crash). Skip cleanly here.
  const std::string &be = Config::get().backend;
  if (be != "d3d11") {
    angle::log("allow_tearing: backend=%s (not d3d11), skipping hooks",
               be.c_str());
    return;
  }

  // Probe driver/OS capability before installing hooks.
  if (!probeTearingSupport()) {
    angle::log("allow_tearing: tearing not supported, skipping hooks");
    return;
  }
  g_tearingSupported.store(true, std::memory_order_release);

  // Resolve real DXGI entry points (we'll forward to these from our hooks).
  HMODULE dxgi = GetModuleHandleA("dxgi.dll");
  if (!dxgi)
    dxgi = LoadLibraryA("dxgi.dll");
  if (!dxgi) {
    angle::log("allow_tearing: dxgi.dll missing, abort");
    return;
  }
  real_CreateDXGIFactory1 =
      (PFN_CreateDXGIFactory1)GetProcAddress(dxgi, "CreateDXGIFactory1");
  real_CreateDXGIFactory2 =
      (PFN_CreateDXGIFactory2)GetProcAddress(dxgi, "CreateDXGIFactory2");

  // Install IAT hooks on libGLESv2.dll so subsequent ANGLE calls hit ours.
  // ANGLE may have already created its factory by the time we run (we're
  // invoked from gdangle_postGLInit, after wglMakeCurrent succeeded). In
  // that case the IAT hook still helps if ANGLE re-creates a factory
  // (e.g. on display loss), AND we'll catch the existing swap chain on
  // first SwapBuffers via a separate path. For now: install + log.

  HMODULE gles2 = GetModuleHandleA("libGLESv2.dll");
  if (gles2) {
    void *old1 = iat::hook(gles2, "dxgi.dll", "CreateDXGIFactory1",
                           (void *)&Hook_CreateDXGIFactory1);
    if (old1) {
      if (!real_CreateDXGIFactory1)
        real_CreateDXGIFactory1 = (PFN_CreateDXGIFactory1)old1;
      angle::log(
          "allow_tearing: IAT hook libGLESv2!CreateDXGIFactory1 OK (orig=%p)",
          old1);
    }
    void *old2 = iat::hook(gles2, "dxgi.dll", "CreateDXGIFactory2",
                           (void *)&Hook_CreateDXGIFactory2);
    if (old2) {
      if (!real_CreateDXGIFactory2)
        real_CreateDXGIFactory2 = (PFN_CreateDXGIFactory2)old2;
      angle::log(
          "allow_tearing: IAT hook libGLESv2!CreateDXGIFactory2 OK (orig=%p)",
          old2);
    }
  }

  // Catch already-created swap chain (if ANGLE made one before our hooks).
  // ANGLE exposes the D3D11 device via EGL_D3D11_DEVICE_ANGLE (0x33A1).
  // From the device we can reach the swap chain through DXGI device chain,
  // but this is fragile - easier path: vtable patch is shared, so the FIRST
  // present from any swap chain hits our hook IF we install vtable patch
  // proactively via the factory of the existing swap chain. We do so by
  // walking: D3D11Device -> IDXGIDevice -> GetParent(IDXGIAdapter) ->
  // GetParent(IDXGIFactory).
  auto &a = angle::state();
  if (!a.egl || !a.display) {
    angle::log(
        "allow_tearing: ANGLE not ready, IAT hooks installed for future calls");
    return;
  }

  using QueryDisplayAttribFn = int (*)(void *, int, intptr_t *);
  using QueryDeviceAttribFn = int (*)(void *, int, intptr_t *);
  auto qDisp =
      (QueryDisplayAttribFn)GetProcAddress(a.egl, "eglQueryDisplayAttribEXT");
  auto qDev =
      (QueryDeviceAttribFn)GetProcAddress(a.egl, "eglQueryDeviceAttribEXT");
  if (!qDisp || !qDev) {
    angle::log("allow_tearing: EGL device-query extensions missing");
    return;
  }
  intptr_t eglDevice = 0;
  if (!qDisp(a.display, 0x322C /*EGL_DEVICE_EXT*/, &eglDevice) || !eglDevice) {
    angle::log("allow_tearing: no EGL device");
    return;
  }
  intptr_t d3d11Device = 0;
  if (!qDev((void *)eglDevice, 0x33A1 /*EGL_D3D11_DEVICE_ANGLE*/,
            &d3d11Device) ||
      !d3d11Device) {
    angle::log("allow_tearing: no D3D11 device");
    return;
  }
  // QI for IDXGIDevice
  static const GUID IID_IDXGIDevice = {
      0x54ec77fa,
      0x1377,
      0x44e6,
      {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c}};
  struct IUnknownVtbl {
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(void *, REFIID, void **);
    ULONG(STDMETHODCALLTYPE *AddRef)(void *);
    ULONG(STDMETHODCALLTYPE *Release)(void *);
  };
  auto *devVtbl = *(IUnknownVtbl **)d3d11Device;
  void *dxgiDev = nullptr;
  if (FAILED(devVtbl->QueryInterface((void *)d3d11Device, IID_IDXGIDevice,
                                     &dxgiDev)) ||
      !dxgiDev) {
    angle::log("allow_tearing: device->IDXGIDevice QI failed");
    return;
  }
  // IDXGIDevice::GetAdapter at slot 7
  using GetAdapterFn = HRESULT(STDMETHODCALLTYPE *)(void *, void **);
  auto **dxgiDevVtbl = *(void ***)dxgiDev;
  auto getAdapter = (GetAdapterFn)dxgiDevVtbl[7];
  void *adapter = nullptr;
  HRESULT hr = getAdapter(dxgiDev, &adapter);
  auto *dxgiDevVtblIUnk = *(IUnknownVtbl **)dxgiDev;
  dxgiDevVtblIUnk->Release(dxgiDev);
  if (FAILED(hr) || !adapter) {
    angle::log("allow_tearing: GetAdapter failed");
    return;
  }
  // IDXGIObject::GetParent at slot 6 -> returns parent IDXGIFactory
  using GetParentFn = HRESULT(STDMETHODCALLTYPE *)(void *, REFIID, void **);
  static const GUID IID_IDXGIFactory = {
      0x7b7166ec,
      0x21c7,
      0x44ae,
      {0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69}};
  auto **adVtbl = *(void ***)adapter;
  auto adGetParent = (GetParentFn)adVtbl[6];
  void *factory = nullptr;
  hr = adGetParent(adapter, IID_IDXGIFactory, &factory);
  auto *adVtblIUnk = *(IUnknownVtbl **)adapter;
  adVtblIUnk->Release(adapter);
  if (FAILED(hr) || !factory) {
    angle::log("allow_tearing: GetParent(IDXGIFactory) failed");
    return;
  }
  detourFactoryVtable(factory);
  auto *facVtblIUnk = *(IUnknownVtbl **)factory;
  facVtblIUnk->Release(factory);

  angle::log("allow_tearing: factory vtable detoured for already-existing "
             "ANGLE factory");
}

bool isEnabled() { return g_enabled.load(std::memory_order_acquire); }
bool isTearingSupported() {
  return g_tearingSupported.load(std::memory_order_acquire);
}
} // namespace boost_allow_tearing
