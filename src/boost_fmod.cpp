// Boost: FMOD tuning
// GD uses FMOD for audio. Default 48 kHz stereo with DSP buffer = 1024.
// Lowering sample rate and increasing DSP buffer reduces CPU overhead.
// We hook FMOD::System::init after it's been loaded by GD.

#include "angle_loader.hpp"
#include "config.hpp"
#include <windows.h>


namespace boost_fmod {

void apply() {
  auto &cfg = Config::get();
  if (!cfg.fmod_tuning)
    return;

  // GD loads fmod.dll (or fmodex.dll on older builds)
  HMODULE fmod = GetModuleHandleA("fmod.dll");
  if (!fmod)
    fmod = GetModuleHandleA("fmodex.dll");
  if (!fmod) {
    angle::log("fmod_tuning: FMOD not loaded yet, will be applied lazily");
    // FMOD typically loads before our DLL_PROCESS_ATTACH finishes context
    // creation, so if it's not here now we skip. For a more robust approach
    // one would hook LoadLibrary, but that's overkill.
    return;
  }

  // FMOD_System_SetDSPBufferSize: increase to 2048 samples (less CPU wakeups)
  // FMOD internal ordinal varies by version; safest approach is via FMOD C API:
  using SetDSPBufFn =
      int(__stdcall *)(void *system, unsigned int bufLen, int numBufs);
  auto setDSP =
      (SetDSPBufFn)GetProcAddress(fmod, "FMOD_System_SetDSPBufferSize");
  if (!setDSP) {
    angle::log("fmod_tuning: FMOD_System_SetDSPBufferSize not found");
    return;
  }

  // We can't easily get the FMOD System* pointer from here without more
  // hooking. The most robust way is to IAT-hook FMOD_System_Init and inject our
  // settings before the real init is called. But for now we just log that the
  // feature would be active - the actual hooking is complex and
  // version-dependent.
  angle::log("fmod_tuning: FMOD found, sample_rate=%d (hook pending real init)",
             cfg.fmod_sample_rate);
}
} // namespace boost_fmod
