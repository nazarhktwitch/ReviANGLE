// Boost: disable process mitigations
// Control Flow Guard (CFG) and Shadow Stacks (CET) add ~1-2% overhead on every
// indirect call. Disabling them is a micro-optimisation with SECURITY
// IMPLICATIONS. DISABLED BY DEFAULT.

#include "angle_loader.hpp"
#include "config.hpp"
#include <windows.h>


namespace boost_mitigation_off {

void apply() {
  if (!Config::get().mitigation_off)
    return;

  // ProcessControlFlowGuardPolicy = 7
  // We set EnableControlFlowGuard = 0 to disable CFG.
  struct {
    DWORD EnableControlFlowGuard : 1;
    DWORD StrictMode : 1;
  } cfgPolicy = {0, 0};

  using SetMitigationFn = BOOL(WINAPI *)(HANDLE, int, PVOID, SIZE_T);
  HMODULE k32 = GetModuleHandleA("kernel32.dll");
  auto fn = (SetMitigationFn)GetProcAddress(k32, "SetProcessMitigationPolicy");
  if (!fn) {
    angle::log("mitigation_off: API not available (old Windows?)");
    return;
  }

  // ProcessControlFlowGuardPolicy = 7
  BOOL ok = fn(GetCurrentProcess(), 7, &cfgPolicy, sizeof(cfgPolicy));
  if (ok) {
    angle::log("mitigation_off: CFG disabled");
  } else {
    angle::log(
        "mitigation_off: failed (err=%lu) - CFG was probably enforced by OS",
        GetLastError());
  }
}
} // namespace boost_mitigation_off
