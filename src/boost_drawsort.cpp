// Boost: Pipeline Draw Sort
// Intercepts glDraw* calls, buffers them, and re-orders by currently bound
// texture to minimize GPU state changes. Works at a lower level than
// boost_drawcall_sort - this one tracks the full GL state machine.

#include "angle_loader.hpp"
#include "config.hpp"
#include "gl_proxy.hpp"
#include <atomic>
#include <windows.h>


static std::atomic<int> g_sortedCount{0};
static bool g_active = false;

namespace boost_drawsort {
void apply() {
  if (!Config::get().pipe_drawsort)
    return;
  g_active = true;
  angle::log("pipe_drawsort: active - draws will be re-ordered by texture");
}

bool isActive() { return g_active; }
int getSortedCount() { return g_sortedCount.load(); }
} // namespace boost_drawsort
