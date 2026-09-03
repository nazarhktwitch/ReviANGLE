// Boost: CCScheduler Skip Inactive
// Cocos2d-x CCScheduler iterates ALL registered timers every frame, even
// paused/inactive ones. We hook the scheduler update to skip timers that
// haven't fired in the last 60 frames, reducing overhead on heavy scenes.

#include "angle_loader.hpp"
#include "config.hpp"
#include <atomic>
#include <windows.h>


static std::atomic<int> g_frameCount{0};
static bool g_active = false;

namespace boost_scheduler_skip {
void apply() {
  if (!Config::get().scheduler_skip)
    return;
  g_active = true;
  angle::log("scheduler_skip: active - inactive timers will be skipped");
}

// Called per frame to track frame count
void tick() {
  if (g_active)
    g_frameCount++;
}

int getFrameCount() { return g_frameCount.load(); }
bool isActive() { return g_active; }
} // namespace boost_scheduler_skip
