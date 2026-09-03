// Boost: Disable Anti-Aliasing
// MSAA is extremely expensive on low-end GPUs like GT 630M - it multiplies
// fill rate cost by the sample count. GD's pixel art style doesn't benefit
// from AA. We intercept glEnable(GL_MULTISAMPLE) and force MSAA samples to 0.

#include "angle_loader.hpp"
#include "config.hpp"
#include "gl_proxy.hpp"
#include <windows.h>


typedef unsigned int GLenum;
typedef int GLint;

constexpr GLenum GL_MULTISAMPLE = 0x809D;

using EnableFn = void(WINAPI *)(GLenum);
using DisableFn = void(WINAPI *)(GLenum);
using GetIntegervFn = void(WINAPI *)(GLenum, GLint *);

static EnableFn s_origEnable = nullptr;
static DisableFn s_origDisable = nullptr;
static bool g_active = false;

static void WINAPI hooked_glEnable(GLenum cap) {
  if (g_active && cap == GL_MULTISAMPLE) {
    // Silently ignore - keep MSAA disabled
    return;
  }
  s_origEnable(cap);
}

namespace boost_no_aa {

void apply() {
  if (!Config::get().disable_aa)
    return;

  s_origEnable = (EnableFn)glproxy::resolve("glEnable");
  s_origDisable = (DisableFn)glproxy::resolve("glDisable");

  if (s_origEnable && s_origDisable) {
    // Force disable MSAA right now
    s_origDisable(GL_MULTISAMPLE);
    g_active = true;
    angle::log("no_aa: MSAA disabled");
  }
}

void *getEnableHook() { return g_active ? (void *)hooked_glEnable : nullptr; }
} // namespace boost_no_aa
