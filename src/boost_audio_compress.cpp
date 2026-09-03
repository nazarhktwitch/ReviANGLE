// Boost: Audio RAM Compression
// GD decompresses audio into raw PCM in memory. A 3-minute song at 44.1kHz
// stereo 16-bit = ~30MB. We could compress idle audio buffers with a simple
// codec (ADPCM) to ~7.5MB, decompressing on-demand.
//
// For now, this module just frees unused FMOD sound data after playback ends.

#include "angle_loader.hpp"
#include "config.hpp"
#include <atomic>
#include <windows.h>


static std::atomic<size_t> g_saved{0};
static bool g_active = false;

namespace boost_audio_compress {
void apply() {
  if (!Config::get().audio_ram_compress)
    return;
  g_active = true;
  angle::log("audio_compress: active - will free unused audio buffers");
}

void onSoundStop(void *buffer, size_t size) {
  if (!g_active || !buffer)
    return;
  // Mark pages as not needed - OS can reclaim without page fault cost later
  VirtualAlloc(buffer, size, MEM_RESET, PAGE_READWRITE);
  g_saved += size;
}

size_t getSavedBytes() { return g_saved.load(); }
bool isActive() { return g_active; }
} // namespace boost_audio_compress
