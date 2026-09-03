// Boost: Audio Thread Pinning
// Pin FMOD's internal mixing thread to a specific CPU core, preventing it
// from competing with the render thread and reducing context switch overhead.

#include "angle_loader.hpp"
#include "config.hpp"
#include <thread>
#include <tlhelp32.h>
#include <windows.h>


namespace {

void pinAudioThread() {
  // Wait for FMOD to create its threads
  Sleep(2000);

  DWORD pid = GetCurrentProcessId();
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return;

  THREADENTRY32 te;
  te.dwSize = sizeof(te);

  // FMOD typically creates 1-2 threads. We look for threads with
  // lower priority (FMOD mixer runs at ABOVE_NORMAL or HIGH).
  int pinned = 0;
  if (Thread32First(snap, &te)) {
    do {
      if (te.th32OwnerProcessID != pid)
        continue;
      if (te.th32ThreadID == GetCurrentThreadId())
        continue;

      HANDLE h = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
      if (!h)
        continue;

      int prio = GetThreadPriority(h);
      if (prio >= THREAD_PRIORITY_ABOVE_NORMAL) {
        // Likely FMOD thread - pin to last core
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD lastCore = si.dwNumberOfProcessors - 1;
        SetThreadAffinityMask(h, 1ULL << lastCore);
        pinned++;
      }
      CloseHandle(h);
    } while (Thread32Next(snap, &te));
  }

  CloseHandle(snap);
  if (pinned > 0) {
    angle::log("audio_pin: pinned %d audio thread(s) to last core", pinned);
  }
}

} // namespace

namespace boost_audio_pin {
void apply() {
  if (!Config::get().audio_thread_pin)
    return;

  std::thread(pinAudioThread).detach();
  angle::log("audio_pin: will pin FMOD threads after 2s");
}
} // namespace boost_audio_pin
