// Boost: Anti-stutter
//
// Targets micro-jitter sources outside the renderer that the OS introduces:
//
//   1. Process affinity update mode - by default Windows is allowed to migrate
//      the process between cores when CPU pressure changes. Each migration
//      flushes L1/L2/TLB and costs 0.5–2 ms. Locking the mode disables it.
//
//   2. Ideal processor - sets the *preferred* core for the main thread so the
//      Windows scheduler has a strong hint to keep us pinned. Combined with
//      smart_cpu_pin's affinity mask this reduces context-switch jitter.
//
//   3. Foreground priority boost - by default GUI apps that lose focus drop
//      priority class (UI threads sleep more). Disabling background-mode
//      ensures stable scheduling even if the user clicks away briefly.
//
//   4. Process protection from background services - disable the "Modern
//      Standby" Eco-throttle (PROCESS_POWER_THROTTLING) and the Energy-Aware
//      scheduler hints. boost_power already covers part of this; we re-apply
//      defensively in case the OS toggles state when GD goes fullscreen.
//
// All calls are no-ops on systems where the API is missing (older Win10 1709+).

#include "angle_loader.hpp"
#include "config.hpp"
#include <windows.h>


// SetProcessAffinityUpdateMode is exported from kernel32.dll on Win7+.
using SetAffinityUpdateModeFn = BOOL(WINAPI *)(HANDLE, DWORD);
#ifndef PROCESS_AFFINITY_DISABLE_AUTO_UPDATE
#define PROCESS_AFFINITY_DISABLE_AUTO_UPDATE 0x1
#endif

// SetThreadInformation: ThreadPowerThrottling = 4 (disable Eco-QoS for thread).
#ifndef ThreadPowerThrottling
#define ThreadPowerThrottling 4
#endif
#ifndef THREAD_POWER_THROTTLING_EXECUTION_SPEED
#define THREAD_POWER_THROTTLING_EXECUTION_SPEED 0x1
#endif
#ifndef THREAD_POWER_THROTTLING_CURRENT_VERSION
#define THREAD_POWER_THROTTLING_CURRENT_VERSION 1
#endif
typedef struct _MY_THREAD_POWER_THROTTLING_STATE {
  ULONG Version;
  ULONG ControlMask;
  ULONG StateMask;
} MY_THREAD_POWER_THROTTLING_STATE;
using SetThreadInformationFn = BOOL(WINAPI *)(HANDLE, int, PVOID, ULONG);

namespace boost_anti_stutter {

void apply() {
  if (!Config::get().anti_stutter) {
    angle::log("anti_stutter: disabled by config");
    return;
  }

  HMODULE k32 = GetModuleHandleA("kernel32.dll");
  if (!k32)
    return;

  // 1. Lock affinity update mode - prevents Windows from migrating the
  //    process across cores when load shifts (each migration = TLB/cache
  //    flush = 0.5–2 ms hitch).
  auto setAffMode = (SetAffinityUpdateModeFn)GetProcAddress(
      k32, "SetProcessAffinityUpdateMode");
  if (setAffMode) {
    if (setAffMode(GetCurrentProcess(), PROCESS_AFFINITY_DISABLE_AUTO_UPDATE)) {
      angle::log("anti_stutter: affinity auto-update disabled");
    } else {
      angle::log("anti_stutter: SetProcessAffinityUpdateMode failed (gle=%lu)",
                 GetLastError());
    }
  }

  // 2. Set the ideal processor for the main render thread. Combined with
  //    smart_cpu_pin's mask this gives the scheduler the strongest hint
  //    possible to keep us on one core's L1/L2.
  DWORD_PTR procMask = 0, sysMask = 0;
  if (GetProcessAffinityMask(GetCurrentProcess(), &procMask, &sysMask) &&
      procMask) {
    // Pick the lowest set bit (avoids hyperthread sibling on most CPUs).
    DWORD ideal = 0;
    for (DWORD i = 0; i < 64; ++i) {
      if (procMask & (DWORD_PTR(1) << i)) {
        ideal = i;
        break;
      }
    }
    DWORD prevIdeal = SetThreadIdealProcessor(GetCurrentThread(), ideal);
    if (prevIdeal != (DWORD)-1) {
      angle::log(
          "anti_stutter: ideal processor for main thread = %lu (was %lu)",
          ideal, prevIdeal);
    }
  }

  // 3. Disable per-thread Eco-QoS power throttling. Even if the process
  //    has it disabled, individual threads can still be throttled when
  //    the OS thinks they're "background work" (e.g. between presents
  //    where the thread waits on the GPU).
  auto setThrInfo =
      (SetThreadInformationFn)GetProcAddress(k32, "SetThreadInformation");
  if (setThrInfo) {
    MY_THREAD_POWER_THROTTLING_STATE st = {};
    st.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
    st.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
    st.StateMask = 0; // 0 = HighQoS / no throttling
    if (setThrInfo(GetCurrentThread(), ThreadPowerThrottling, &st,
                   sizeof(st))) {
      angle::log("anti_stutter: thread power throttling disabled (HighQoS)");
    }
  }

  // 4. Disable foreground/background priority quantum stretching.
  SetProcessPriorityBoost(GetCurrentProcess(), FALSE);
  SetThreadPriorityBoost(GetCurrentThread(), FALSE);

  // 5. Aggressive anti-stutter: Process HIGH_PRIORITY_CLASS, Thread
  // TIME_CRITICAL
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
  angle::log(
      "anti_stutter: aggressive mode (HIGH_PRIORITY_CLASS + TIME_CRITICAL)");
}
} // namespace boost_anti_stutter
