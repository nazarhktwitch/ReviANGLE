// Boost: disable depth testing
// GD is a 2D game - depth test / depth clear are wasted GPU work.
// We intercept glEnable(GL_DEPTH_TEST) and mask GL_DEPTH_BUFFER_BIT from
// glClear.

#include "angle_loader.hpp"
#include "config.hpp"
#include "gl_proxy.hpp"
#include <windows.h>


typedef unsigned int GLenum;
typedef unsigned int GLbitfield;

constexpr GLenum GL_DEPTH_TEST = 0x0B71;
constexpr GLbitfield GL_DEPTH_BUFFER_BIT = 0x00000100;

using EnableFn = void(WINAPI *)(GLenum);
using ClearFn = void(WINAPI *)(GLbitfield);
using DepthMaskFn = void(WINAPI *)(unsigned char);

static EnableFn s_origEnable = nullptr;
static ClearFn s_origClear = nullptr;
static DepthMaskFn s_origDepthMask = nullptr;
static bool g_active = false;

static void WINAPI hooked_glEnable(GLenum cap) {
  if (g_active && cap == GL_DEPTH_TEST)
    return; // skip
  s_origEnable(cap);
}

static void WINAPI hooked_glClear(GLbitfield mask) {
  if (g_active)
    mask &= ~GL_DEPTH_BUFFER_BIT; // strip depth clear
  s_origClear(mask);
}

static void WINAPI hooked_glDepthMask(unsigned char flag) {
  if (g_active) {
    s_origDepthMask(0);
    return;
  }
  s_origDepthMask(flag);
}

namespace boost_depth_off {

void apply() {
  if (!Config::get().depth_off)
    return;

  s_origEnable = (EnableFn)glproxy::resolve("glEnable");
  s_origClear = (ClearFn)glproxy::resolve("glClear");
  s_origDepthMask = (DepthMaskFn)glproxy::resolve("glDepthMask");

  if (s_origEnable && s_origClear) {
    g_active = true;
    angle::log("depth_off: active - depth test/clear disabled");
  }
}

void *getEnableHook() { return g_active ? (void *)hooked_glEnable : nullptr; }
void *getClearHook() { return g_active ? (void *)hooked_glClear : nullptr; }
void *getDepthMaskHook() {
  return g_active ? (void *)hooked_glDepthMask : nullptr;
}
} // namespace boost_depth_off
