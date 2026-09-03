// Boost: Working Set Lock (HARD min)
// ------------------------------------------------------------------------
// Anti-stutter for RAM-tight systems (8 GB with browser/IDE running in
// parallel). Forces Windows to keep our minimum working set resident in
// physical memory - no page-outs during gameplay, no minor-page-fault
// stutters when other apps demand RAM.
//
// SetProcessWorkingSetSize alone is a *soft hint* - Windows can ignore it
// under memory pressure. The Ex variant with QUOTA_LIMITS_HARDWS_MIN_ENABLE
// makes the minimum a HARD floor that the OS won't trim below.
//
// SE_INC_WORKING_SET_NAME privilege is required for the HARD flag. The
// privilege is normally granted to interactive users but disabled in the
// token by default - we enable it via AdjustTokenPrivileges.
//
// Falls back to the soft hint if privilege adjustment fails.

#include "angle_loader.hpp"
#include "config.hpp"
#include <windows.h>


static bool enableSeIncWorkingSet() {
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(),
                        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
    return false;
  }

  LUID luid = {};
  bool ok = false;
  // Use explicit wide literal - TEXT() macro expands to char* in ANSI build.
  if (LookupPrivilegeValueW(nullptr, L"SeIncreaseWorkingSetPrivilege", &luid)) {
    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    ok = (GetLastError() == ERROR_SUCCESS);
  }
  CloseHandle(token);
  return ok;
}

namespace boost_workingset_lock {

void apply() {
  if (!Config::get().workingset_lock)
    return;

#ifndef QUOTA_LIMITS_HARDWS_MIN_ENABLE
#define QUOTA_LIMITS_HARDWS_MIN_ENABLE 0x00000001
#endif
#ifndef QUOTA_LIMITS_HARDWS_MAX_DISABLE
#define QUOTA_LIMITS_HARDWS_MAX_DISABLE 0x00000008
#endif

  struct Pair {
    SIZE_T minMb;
    SIZE_T maxMb;
  };
#ifdef _WIN64
  // x64 process has vast virtual address space.
  // Provide large headroom and disable hard max ceiling.
  const Pair attempts[] = {
      {1024, 16384}, // 1 GB floor, 16 GB ceiling fallback
      {512, 8192},   // 512 MB floor, 8 GB ceiling fallback
      {256, 4096},   // 256 MB floor, 4 GB ceiling fallback
  };
#else
  // x86 process => max addressable ~4 GB (with /LARGEADDRESSAWARE).
  const Pair attempts[] = {
      {384, 1536}, // ideal: 384 MB hot pages locked, 1.5 GB ceiling
      {256, 1024}, // fallback if 384 too tight
      {192, 768},  // last resort
  };
#endif

  const bool privOk = enableSeIncWorkingSet();
  // Use MIN_ENABLE to enforce the floor, and MAX_DISABLE to ensure the OS never
  // caps our maximum working set under memory pressure.
  const DWORD flags =
      privOk
          ? (QUOTA_LIMITS_HARDWS_MIN_ENABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE)
          : 0;

  for (const auto &a : attempts) {
    SIZE_T minWs = a.minMb * 1024u * 1024u;
    SIZE_T maxWs = a.maxMb * 1024u * 1024u;
    if (SetProcessWorkingSetSizeEx(GetCurrentProcess(), minWs, maxWs, flags)) {
      angle::log("workingset_lock: %s floor=%zu MB ceiling=%zu MB",
                 privOk ? "HARD" : "soft", a.minMb, a.maxMb);
      return;
    }
  }

  angle::log("workingset_lock: SetProcessWorkingSetSizeEx failed (err=%lu)",
             GetLastError());
}
} // namespace boost_workingset_lock
