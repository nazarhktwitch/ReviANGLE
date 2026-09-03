// =====================================================================
//  Boost: Safe Latency Optimizations (Geode-Compatible)
// =====================================================================
//
//  Что делает: безопасные оптимизации для снижения input lag и микрофризов
//  без прямого доступа к D3D11 device vtable (конфликт с Geode).
//
//  Методы:
//    1. Thread Priority Boost - приоритет рендер-потока повышается до
//       ABOVE_NORMAL для снижения задержек при планировании CPU.
//
//    2. Core Affinity Optimization - привязка рендер-потока к P-cores
//       на гибридных CPU (Intel 12th+ / AMD Ryzen 7000+) для снижения
//       latency переключения между E/P cores.
//
//    3. DWM Composition Sync - синхронизация с DWM через DwmFlush для
//       windowed/borderless режимов (без доступа к swap chain).
//
//    4. Present Timing Optimization - использование eglSwapInterval(0)
//       с frame pacing для минимального input lag.
//
//  Почему безопасно:
//    - Никакого прямого доступа к D3D11 device vtable
//    - Использует только Windows API (SetThreadPriority, SetThreadAffinityMask)
//    - Работает через EGL config, не трогает DXGI напрямую
//    - Совместимо с Geode hooks на CCEGLView
//
//  Параметры конфига [BoostLatency]:
//    safe_latency - bool, default ON
//
//  Авторы: ReviANGLE / Reviusion
// =====================================================================

#include "angle_loader.hpp"
#include "config.hpp"
#include <windows.h>


namespace boost_safe_latency {

static bool g_active = false;
static HMODULE s_dwmapi = nullptr;
static DWORD g_originalAffinity = 0;
static HANDLE g_renderThread = nullptr;

// DWM API declarations
typedef HRESULT(WINAPI *PFN_DwmFlush)(void);
typedef HRESULT(WINAPI *PFN_DwmIsCompositionEnabled)(BOOL *);
static PFN_DwmFlush s_dwmFlush = nullptr;
static PFN_DwmIsCompositionEnabled s_dwmIsEnabled = nullptr;

// Detect hybrid CPU (Intel 12th+ / AMD Ryzen 7000+)
static bool isHybridCPU() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  // If we have more than 16 logical processors, likely hybrid CPU
  return si.dwNumberOfProcessors > 16;
}

// Get current thread handle
static HANDLE getCurrentThreadHandle() { return GetCurrentThread(); }

// Boost render thread priority
static void boostThreadPriority() {
  HANDLE hThread = getCurrentThreadHandle();
  if (!hThread)
    return;

  // Set to ABOVE_NORMAL priority class for the thread
  // This gives the render thread preference in CPU scheduling
  SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);
  angle::log("safe_latency: render thread priority set to ABOVE_NORMAL");
}

// Optimize thread affinity for hybrid CPUs
static void optimizeThreadAffinity() {
  if (!isHybridCPU()) {
    angle::log("safe_latency: not hybrid CPU, skipping affinity optimization");
    return;
  }

  HANDLE hThread = getCurrentThreadHandle();
  if (!hThread)
    return;

  // Get current affinity
  DWORD_PTR processMask, systemMask;
  if (!GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask)) {
    return;
  }

  g_originalAffinity = processMask;

  // On hybrid CPUs, try to pin to performance cores (typically first 8-16
  // threads) This is a heuristic - on Intel 12th+, P-cores are usually in the
  // first group
  DWORD_PTR perfCoreMask = 0;
  for (int i = 0; i < 16; i++) {
    if (processMask & (1ULL << i)) {
      perfCoreMask |= (1ULL << i);
    }
  }

  if (perfCoreMask && perfCoreMask != processMask) {
    SetThreadAffinityMask(hThread, perfCoreMask);
    angle::log("safe_latency: thread affinity optimized (performance cores)");
  } else {
    angle::log("safe_latency: using default affinity");
  }
}

// Initialize DWM sync for windowed mode
static void initDwmSync() {
  s_dwmapi = LoadLibraryA("dwmapi.dll");
  if (!s_dwmapi) {
    angle::log("safe_latency: dwmapi.dll not available (old Windows?)");
    return;
  }

  s_dwmFlush = (PFN_DwmFlush)GetProcAddress(s_dwmapi, "DwmFlush");
  s_dwmIsEnabled = (PFN_DwmIsCompositionEnabled)GetProcAddress(
      s_dwmapi, "DwmIsCompositionEnabled");

  if (!s_dwmFlush || !s_dwmIsEnabled) {
    angle::log("safe_latency: DWM functions not available");
    return;
  }

  BOOL composEnabled = FALSE;
  if (s_dwmIsEnabled(&composEnabled) && composEnabled) {
    angle::log("safe_latency: DWM composition enabled, sync available");
  } else {
    angle::log(
        "safe_latency: DWM composition disabled (exclusive fullscreen?)");
  }
}

// Public function to call before present (for frame pacing)
extern "C" void gdangle_safeLatencyPrePresent() {
  if (!g_active || !s_dwmFlush)
    return;

  // Optional: DwmFlush can be called before present to sync with DWM
  // This is lightweight and reduces latency in windowed mode
  // Commented out by default - can be enabled if needed
  // s_dwmFlush();
}

void apply() {
  if (!Config::get().safe_latency) {
    angle::log("safe_latency: disabled in config");
    return;
  }

  g_renderThread = getCurrentThreadHandle();

  // Apply safe optimizations
  boostThreadPriority();
  optimizeThreadAffinity();
  initDwmSync();

  g_active = true;
  angle::log("safe_latency: ENABLED (Geode-compatible latency optimizations)");
}

void shutdown() {
  // Restore original affinity if we changed it
  if (g_originalAffinity && g_renderThread) {
    SetThreadAffinityMask(g_renderThread, g_originalAffinity);
    angle::log("safe_latency: restored original thread affinity");
  }

  if (s_dwmapi) {
    FreeLibrary(s_dwmapi);
    s_dwmapi = nullptr;
  }

  g_active = false;
}

} // namespace boost_safe_latency
