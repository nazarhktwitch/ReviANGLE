// Boost: Frame Pacing  (high-res waitable-timer edition)
//
// Hard-caps frame rate to either an explicit target (frame_pacing_target) or
// the monitor's refresh rate. Algorithm per frame (called from
// wgl_wglSwapBuffers BEFORE eglSwapBuffers):
//
//   1. Compute time since last present (`elapsed`).
//   2. If elapsed < target_dt:
//        - Block on a HIGH-RESOLUTION waitable timer (~100 µs precision, 0%
//        CPU)
//          for the bulk of the wait, leaving only ~200 µs for the final spin.
//        - Fall back to Sleep+spin on Windows < 10 1803.
//   3. Spin (YieldProcessor) the last sub-millisecond for tight precision.
//   4. Anchor next-frame timestamp to the ideal tick - long-term FPS stays
//   exact.
//
// Why the timer matters on weak / 2-core CPUs (i5-3230M, GT 630M and friends):
// the previous Sleep+spin path burned a whole core on YieldProcessor for up to
// ~remaining ms every frame. On a 2-thread CPU that core was directly competing
// with cocos2d's main thread - visible as "frame_pacing reduces FPS during
// effects". Using the kernel timer keeps the wait truly idle, so the main
// thread gets the full core back.

#include "angle_loader.hpp"
#include "config.hpp"
#include <windows.h>


#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

static LARGE_INTEGER s_freq = {};
static LARGE_INTEGER s_lastPresent = {};
static double s_targetDt = 0.0; // 0 = unlimited
static bool g_active = false;
static int g_targetFps = 0; // resolved cap (0 = unlimited)
static HANDLE s_timer = nullptr;
static bool s_useHighRes = false;

// Reserve the last ~100 µs for the spin loop - timer wakeups can return up to
// ~100 µs late even with HIGH_RESOLUTION, so we still need a tight final spin.
static constexpr double kSpinSlackSec = 0.0001;

// Resolve target FPS: explicit config override > monitor refresh rate > 0.
static int detectTargetFps() {
  int explicitTarget = Config::get().frame_pacing_target;
  if (explicitTarget > 0) {
    angle::log("frame_pacing: explicit target = %d FPS", explicitTarget);
    return explicitTarget;
  }
  DEVMODEA dm = {};
  dm.dmSize = sizeof(dm);
  if (EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &dm)) {
    if (dm.dmDisplayFrequency > 1 && dm.dmDisplayFrequency < 1000) {
      angle::log("frame_pacing: detected monitor refresh = %lu Hz",
                 dm.dmDisplayFrequency);
      return (int)dm.dmDisplayFrequency;
    }
  }
  angle::log("frame_pacing: could not detect refresh, no cap");
  return 0;
}

namespace boost_frame_pacing {

void apply() {
  if (!Config::get().frame_pacing) {
    angle::log("frame_pacing: disabled by config");
    return;
  }

  QueryPerformanceFrequency(&s_freq);
  QueryPerformanceCounter(&s_lastPresent);

  g_targetFps = detectTargetFps();
  if (g_targetFps <= 0) {
    s_targetDt = 0.0;
    return;
  }
  s_targetDt = 1.0 / (double)g_targetFps;

  // Try to allocate a high-resolution waitable timer (Win10 1803+).
  // This is the modern «precise sleep» - ~100 µs granularity, 0% CPU during
  // the wait, no spin contention with the main thread.
  s_timer = CreateWaitableTimerExW(nullptr, nullptr,
                                   CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                   TIMER_ALL_ACCESS);
  if (s_timer) {
    s_useHighRes = true;
    angle::log("frame_pacing: target dt = %.3f ms (= %d FPS), "
               "high-res waitable timer ENABLED (no CPU spin)",
               s_targetDt * 1000.0, g_targetFps);
  } else {
    // Fallback: pre-1803 Win10, Win7/8. Use Sleep+spin path - worse on
    // 2-core CPUs but functional.
    DWORD err = GetLastError();
    angle::log("frame_pacing: target dt = %.3f ms (= %d FPS), "
               "high-res timer unavailable (err=%lu) - Sleep+spin fallback",
               s_targetDt * 1000.0, g_targetFps, err);
  }

  g_active = true;
}

void shutdown() {
  if (s_timer) {
    CloseHandle(s_timer);
    s_timer = nullptr;
  }
  s_useHighRes = false;
  g_active = false;
}

// Called from wgl_wglSwapBuffers BEFORE eglSwapBuffers.
void prePresent() {
  if (!g_active || s_targetDt <= 0.0)
    return;

  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  double elapsed =
      (double)(now.QuadPart - s_lastPresent.QuadPart) / (double)s_freq.QuadPart;
  double remaining = s_targetDt - elapsed;

  bool waited = false;
  if (remaining > kSpinSlackSec) {
    // Phase 1: kernel-blocked wait - 0% CPU, leaves spin slack at the end.
    const double waitTarget = remaining - kSpinSlackSec;

    if (s_useHighRes) {
      // SetWaitableTimer takes negative 100ns intervals (relative).
      LARGE_INTEGER due;
      due.QuadPart = -(LONGLONG)(waitTarget * 10000000.0);
      if (due.QuadPart < 0 &&
          SetWaitableTimer(s_timer, &due, 0, nullptr, nullptr, FALSE)) {
        WaitForSingleObject(s_timer, INFINITE);
      }
    } else {
      // Legacy: Sleep granularity = 1 ms (timeBeginPeriod(1) from
      // boost_timer). Stop sleeping when <0.5 ms left, spin the rest.
      DWORD sleepMs = (DWORD)((waitTarget - 0.0005) * 1000.0);
      if (sleepMs > 0)
        Sleep(sleepMs);
    }

    // Phase 2: tight spin for the last ~200 µs of precision.
    do {
      YieldProcessor();
      QueryPerformanceCounter(&now);
      elapsed = (double)(now.QuadPart - s_lastPresent.QuadPart) /
                (double)s_freq.QuadPart;
    } while (elapsed < s_targetDt);
    waited = true;
  }

  if (waited) {
    // Anchor to ideal tick - long-term FPS stays exact, no drift.
    s_lastPresent.QuadPart += (LONGLONG)(s_targetDt * (double)s_freq.QuadPart);
    // Catch-up: if we've fallen >2 frames behind real time, realign.
    if (now.QuadPart - s_lastPresent.QuadPart >
        (LONGLONG)(s_targetDt * 2.0 * (double)s_freq.QuadPart)) {
      s_lastPresent = now;
    }
  } else {
    // GPU/CPU couldn't reach target (we're slower than target_dt).
    // Anchor to NOW so negative drift doesn't accumulate - otherwise
    // we'd periodically spike when the budget catches up.
    s_lastPresent = now;
  }
}

bool isActive() { return g_active; }
int getTargetFps() { return g_targetFps; }
} // namespace boost_frame_pacing
