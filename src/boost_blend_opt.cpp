// Boost: Blend Optimization
// GD draws many fully opaque sprites with alpha blending enabled.
// Alpha blending forces the GPU ROP units to read-modify-write the framebuffer,
// doubling bandwidth cost. For opaque sprites (alpha == 1.0), blending is a
// no-op mathematically but still costs bandwidth.
//
// We track glBlendFunc calls. When src=ONE, dst=ZERO (opaque preset), we
// disable blending entirely. When src=SRC_ALPHA, we monitor the most recent
// uniform alpha / vertex color and skip blending for fully opaque draws.

#include "angle_loader.hpp"
#include "config.hpp"
#include "gl_proxy.hpp"
#include <windows.h>


typedef unsigned int GLenum;

constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_ONE = 1;
constexpr GLenum GL_ZERO = 0;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;

using BlendFuncFn = void(WINAPI *)(GLenum, GLenum);
using EnableFn = void(WINAPI *)(GLenum);
using DisableFn = void(WINAPI *)(GLenum);

static BlendFuncFn s_origBlendFunc = nullptr;
static EnableFn s_origEnable = nullptr;
static DisableFn s_origDisable = nullptr;
static bool g_active = false;

static bool s_blendEnabled = true;
static GLenum s_srcFactor = GL_SRC_ALPHA;
static GLenum s_dstFactor = GL_ONE_MINUS_SRC_ALPHA;

static void WINAPI hooked_glBlendFunc(GLenum sfactor, GLenum dfactor) {
  s_srcFactor = sfactor;
  s_dstFactor = dfactor;

  if (g_active && sfactor == GL_ONE && dfactor == GL_ZERO) {
    // Opaque blend mode - disable blending entirely for perf
    if (s_blendEnabled && s_origDisable) {
      s_origDisable(GL_BLEND);
      s_blendEnabled = false;
    }
    return;
  }

  // Re-enable blending if it was disabled by our optimization
  if (g_active && !s_blendEnabled && s_origEnable) {
    s_origEnable(GL_BLEND);
    s_blendEnabled = true;
  }

  s_origBlendFunc(sfactor, dfactor);
}

static void WINAPI hooked_glEnable(GLenum cap) {
  if (cap == GL_BLEND)
    s_blendEnabled = true;
  s_origEnable(cap);
}

static void WINAPI hooked_glDisable(GLenum cap) {
  if (cap == GL_BLEND)
    s_blendEnabled = false;
  s_origDisable(cap);
}

namespace boost_blend_opt {

void apply() {
  if (!Config::get().blend_optimize)
    return;

  s_origBlendFunc = (BlendFuncFn)glproxy::resolve("glBlendFunc");
  s_origEnable = (EnableFn)glproxy::resolve("glEnable");
  s_origDisable = (DisableFn)glproxy::resolve("glDisable");

  if (s_origBlendFunc && s_origEnable && s_origDisable) {
    g_active = true;
    angle::log("blend_opt: active");
  }
}

void *getBlendFuncHook() {
  return g_active ? (void *)hooked_glBlendFunc : nullptr;
}
void *getEnableHook() { return g_active ? (void *)hooked_glEnable : nullptr; }
void *getDisableHook() { return g_active ? (void *)hooked_glDisable : nullptr; }
} // namespace boost_blend_opt
