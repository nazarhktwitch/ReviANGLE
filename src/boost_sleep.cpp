// Boost: precise sleep
// Replaces Windows Sleep() with a high-resolution WaitableTimer (0% CPU idle)
// without burning CPU in spin loops or starving driver worker threads.

#include <windows.h>
#include "config.hpp"
#include "common/iat_hook.hpp"
#include "angle_loader.hpp"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

using SleepFn = void (WINAPI*)(DWORD);
static SleepFn s_origSleep = nullptr;
static HANDLE  s_timer     = nullptr;

static void WINAPI preciseSleep(DWORD ms) {
    if (ms == 0) {
        if (s_origSleep) s_origSleep(0);
        else Sleep(0);
        return;
    }

    if (s_timer) {
        LARGE_INTEGER due;
        due.QuadPart = -(LONGLONG)ms * 10000LL;  // 100ns units, negative = relative
        if (SetWaitableTimer(s_timer, &due, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(s_timer, ms + 50);
            return;
        }
    }

    if (s_origSleep) s_origSleep(ms);
    else Sleep(ms);
}

namespace boost_sleep {

    void apply() {
        if (!Config::get().precise_sleep) return;

        // Try high-resolution waitable timer (Win10 1803+), fallback to standard auto-reset timer
        s_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!s_timer) {
            s_timer = CreateWaitableTimerA(nullptr, FALSE, nullptr); // bManualReset = FALSE
        }

        s_origSleep = (SleepFn)iat::hookInMainExe("kernel32.dll", "Sleep", (void*)preciseSleep);

        if (s_origSleep) {
            angle::log("precise_sleep: active (IAT hooked Sleep with waitable timer)");
        } else {
            angle::log("precise_sleep: IAT hook failed");
        }
    }

    void shutdown() {
        if (s_timer) { CloseHandle(s_timer); s_timer = nullptr; }
    }
}
