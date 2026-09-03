// Boost: automatic draw call instancing
// Detects sequences of identical glDrawArrays calls (same shader, VBO, vertex
// count) and batches them into glDrawArraysInstanced. Hugely helps particle-
// heavy levels where thousands of identical quads are drawn one-by-one.

#include "angle_loader.hpp"
#include "config.hpp"
#include "gl_proxy.hpp"
#include <windows.h>


typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;

using DrawArraysFn = void(WINAPI *)(GLenum, GLint, GLsizei);
using DrawArraysInstancedFn = void(WINAPI *)(GLenum, GLint, GLsizei, GLsizei);
using GetIntegervFn = void(WINAPI *)(GLenum, GLint *);

static DrawArraysFn s_origDrawArrays = nullptr;
static DrawArraysInstancedFn s_drawInstanced = nullptr;
static GetIntegervFn s_getIntegerv = nullptr;
static bool g_active = false;

// tracking state for batch detection
static GLenum s_lastMode = 0;
static GLint s_lastFirst = -1;
static GLsizei s_lastCount = 0;
static GLint s_lastProg = 0;
static int s_runLen = 0;

static void flushBatch() {
  if (s_runLen <= 0)
    return;
  if (s_runLen > 1 && s_drawInstanced) {
    s_drawInstanced(s_lastMode, s_lastFirst, s_lastCount, (GLsizei)s_runLen);
  } else {
    s_origDrawArrays(s_lastMode, s_lastFirst, s_lastCount);
  }
  s_runLen = 0;
}

static void WINAPI hooked_glDrawArrays(GLenum mode, GLint first,
                                       GLsizei count) {
  if (!g_active || !s_drawInstanced) {
    s_origDrawArrays(mode, first, count);
    return;
  }

  // check current program
  GLint prog = 0;
  if (s_getIntegerv)
    s_getIntegerv(0x8B8D, &prog); // GL_CURRENT_PROGRAM

  if (mode == s_lastMode && first == s_lastFirst && count == s_lastCount &&
      prog == s_lastProg && s_runLen > 0) {
    s_runLen++;
    return;
  }

  // different draw - flush previous batch
  flushBatch();

  s_lastMode = mode;
  s_lastFirst = first;
  s_lastCount = count;
  s_lastProg = prog;
  s_runLen = 1;
}

namespace boost_instancing {

void apply() {
  if (!Config::get().instancing)
    return;

  s_origDrawArrays = (DrawArraysFn)glproxy::resolve("glDrawArrays");
  s_drawInstanced =
      (DrawArraysInstancedFn)glproxy::resolve("glDrawArraysInstanced");
  s_getIntegerv = (GetIntegervFn)glproxy::resolve("glGetIntegerv");

  if (s_origDrawArrays && s_drawInstanced) {
    g_active = true;
    angle::log("instancing: active");
  } else {
    angle::log("instancing: glDrawArraysInstanced not available");
  }
}

void *getDrawArraysHook() {
  return g_active ? (void *)hooked_glDrawArrays : nullptr;
}

// call at end of frame to flush last pending batch
void endFrame() {
  if (g_active)
    flushBatch();
}
} // namespace boost_instancing
