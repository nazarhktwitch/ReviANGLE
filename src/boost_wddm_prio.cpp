// Boost: WDDM GPU scheduling priority
// Sets the GD process to HIGH GPU scheduling priority, making the GPU
// prioritise our work over background tasks (desktop compositor, other apps).

#include "angle_loader.hpp"
#include "config.hpp"
#include <windows.h>


// D3DKMT types
typedef UINT D3DKMT_HANDLE;
typedef enum {
  D3DKMT_SCHEDULINGPRIORITYCLASS_IDLE,
  D3DKMT_SCHEDULINGPRIORITYCLASS_BELOW_NORMAL,
  D3DKMT_SCHEDULINGPRIORITYCLASS_NORMAL,
  D3DKMT_SCHEDULINGPRIORITYCLASS_ABOVE_NORMAL,
  D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH,
  D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME
} D3DKMT_SCHEDULINGPRIORITYCLASS;

using SetSchedPrioFn = LONG(WINAPI *)(D3DKMT_HANDLE,
                                      D3DKMT_SCHEDULINGPRIORITYCLASS);

namespace boost_wddm_prio {

void apply() {
  if (!Config::get().wddm_priority)
    return;

  HMODULE gdi32 = GetModuleHandleA("gdi32.dll");
  if (!gdi32)
    gdi32 = LoadLibraryA("gdi32.dll");
  if (!gdi32)
    return;

  auto fn = (SetSchedPrioFn)GetProcAddress(
      gdi32, "D3DKMTSetProcessSchedulingPriorityClass");
  if (!fn) {
    angle::log("wddm_prio: D3DKMTSetProcessSchedulingPriorityClass not found");
    return;
  }

  LONG result = fn((D3DKMT_HANDLE)(uintptr_t)GetCurrentProcess(),
                   D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH);
  if (result == 0) {
    angle::log("wddm_prio: GPU priority set to HIGH");
  } else {
    angle::log("wddm_prio: failed (0x%lx) - may need admin", result);
  }
}
} // namespace boost_wddm_prio
