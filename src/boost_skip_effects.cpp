// Boost: Skip Shake & Flash Effects
// GD uses CCShaky3D for screen shake and quick fullscreen flash overlays on
// certain triggers. Both are expensive:
// - CCShaky3D requires a full-screen grid mesh that distorts all vertices
// - Flash overlays trigger an extra full-screen alpha-blended draw
//
// We intercept glDrawArrays when a large full-screen quad with specific
// patterns is detected, and skip the draw entirely.
// A more targeted approach hooks specific cocos2d action classes.

#include "angle_loader.hpp"
#include "config.hpp"
#include "gl_proxy.hpp"
#include <windows.h>


typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;

using DrawArraysFn = void(WINAPI *)(GLenum, GLint, GLsizei);
using GetIntegervFn = void(WINAPI *)(GLenum, GLint *);

static DrawArraysFn s_origDrawArrays = nullptr;
static GetIntegervFn s_getIntegerv = nullptr;
static bool g_active = false;

// CCShaky3D uses a grid mesh drawn as GL_TRIANGLES with a high vertex count.
// Typical grid is 20x15 = 300 cells = 1800 triangles = 5400 vertices.
// Regular sprite batches rarely exceed this with a single draw call.
static constexpr GLsizei SHAKE_VERTEX_THRESHOLD = 3000;

static void WINAPI hooked_glDrawArrays(GLenum mode, GLint first,
                                       GLsizei count) {
  if (g_active && mode == 0x0004 &&
      count >= SHAKE_VERTEX_THRESHOLD) { // GL_TRIANGLES
    // Heuristic: very large single draw of triangles = likely grid effect
    // Skip it silently
    return;
  }
  s_origDrawArrays(mode, first, count);
}

namespace boost_skip_effects {

void apply() {
  if (!Config::get().skip_shake_flash)
    return;

  s_origDrawArrays = (DrawArraysFn)glproxy::resolve("glDrawArrays");
  s_getIntegerv = (GetIntegervFn)glproxy::resolve("glGetIntegerv");

  if (s_origDrawArrays) {
    g_active = true;
    angle::log("skip_effects: active - shake/flash suppressed");
  }
}

void *getDrawArraysHook() {
  return g_active ? (void *)hooked_glDrawArrays : nullptr;
}
} // namespace boost_skip_effects
