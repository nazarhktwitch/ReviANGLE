// Boost: Disable ETW (Event Tracing for Windows)
// Windows generates ETW events for many operations (file I/O, registry,
// network). This adds overhead even when no consumer is listening. We disable
// the most expensive providers for our process.

#include "angle_loader.hpp"
#include "common/iat_hook.hpp"
#include "config.hpp"
#include <windows.h>


// Hook NtTraceEvent to no-op it for our process
using EventWriteFn = ULONG(WINAPI *)(ULONGLONG, PVOID, ULONG, PVOID);
static EventWriteFn s_origEventWrite = nullptr;
static bool g_active = false;

static ULONG WINAPI hooked_EventWrite(ULONGLONG handle, PVOID desc, ULONG count,
                                      PVOID data) {
  if (g_active)
    return 0; // silently succeed without tracing
  return s_origEventWrite(handle, desc, count, data);
}

namespace boost_etw_off {
void apply() {
  if (!Config::get().etw_disable)
    return;

  // Hook EventWrite in advapi32
  HMODULE exe = GetModuleHandleA(nullptr);
  s_origEventWrite = (EventWriteFn)iat::hook(exe, "advapi32.dll", "EventWrite",
                                             (void *)hooked_EventWrite);

  if (s_origEventWrite) {
    g_active = true;
    angle::log("etw_off: ETW EventWrite hooked - tracing disabled");
  } else {
    // Try ntdll's EtwEventWrite
    s_origEventWrite = (EventWriteFn)iat::hook(
        exe, "ntdll.dll", "EtwEventWrite", (void *)hooked_EventWrite);
    if (s_origEventWrite) {
      g_active = true;
      angle::log("etw_off: ntdll EtwEventWrite hooked");
    } else {
      angle::log("etw_off: no ETW functions found to hook");
    }
  }
}
} // namespace boost_etw_off
