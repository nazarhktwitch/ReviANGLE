// Boost: Draw Call Sort
// GD renders sprites in scene graph order, which often alternates between
// different textures/shaders. This causes expensive state changes on the GPU.
// We buffer draw commands and sort them by texture ID before flushing,
// minimizing state switches.

#include "angle_loader.hpp"
#include "config.hpp"
#include "gl_proxy.hpp"
#include <algorithm>
#include <vector>
#include <windows.h>


typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;

using DrawArraysFn = void(WINAPI *)(GLenum, GLint, GLsizei);
using BindTexFn = void(WINAPI *)(GLenum, GLuint);
using GetIntegervFn = void(WINAPI *)(GLenum, GLint *);

static DrawArraysFn s_origDrawArrays = nullptr;
static BindTexFn s_origBindTex = nullptr;
static GetIntegervFn s_getIntegerv = nullptr;
static bool g_active = false;

namespace {

struct DrawCmd {
  GLuint texId;
  GLenum mode;
  GLint first;
  GLsizei count;
};

std::vector<DrawCmd> g_drawBuffer;
GLuint g_currentTexId = 0;

void flushSorted() {
  if (g_drawBuffer.empty())
    return;

  // Sort by texture ID
  std::sort(
      g_drawBuffer.begin(), g_drawBuffer.end(),
      [](const DrawCmd &a, const DrawCmd &b) { return a.texId < b.texId; });

  GLuint lastTex = ~0u;
  for (auto &cmd : g_drawBuffer) {
    if (cmd.texId != lastTex) {
      s_origBindTex(0x0DE1, cmd.texId); // GL_TEXTURE_2D
      lastTex = cmd.texId;
    }
    s_origDrawArrays(cmd.mode, cmd.first, cmd.count);
  }
  g_drawBuffer.clear();
}

} // namespace

static void WINAPI hooked_glDrawArrays(GLenum mode, GLint first,
                                       GLsizei count) {
  if (g_active && mode == 0x0004) { // GL_TRIANGLES only
    g_drawBuffer.push_back({g_currentTexId, mode, first, count});
    if (g_drawBuffer.size() >= 256)
      flushSorted();
    return;
  }
  flushSorted();
  s_origDrawArrays(mode, first, count);
}

static void WINAPI hooked_glBindTexture(GLenum target, GLuint tex) {
  if (g_active && target == 0x0DE1) {
    g_currentTexId = tex;
    // Don't call real bind yet - deferred to flush
    return;
  }
  s_origBindTex(target, tex);
}

namespace boost_drawcall_sort {
void apply() {
  if (!Config::get().drawcall_sort)
    return;

  s_origDrawArrays = (DrawArraysFn)glproxy::resolve("glDrawArrays");
  s_origBindTex = (BindTexFn)glproxy::resolve("glBindTexture");
  s_getIntegerv = (GetIntegervFn)glproxy::resolve("glGetIntegerv");

  if (s_origDrawArrays && s_origBindTex) {
    g_drawBuffer.reserve(256);
    g_active = true;
    angle::log("drawcall_sort: active");
  }
}

void flush() { flushSorted(); }
void *getDrawArraysHook() {
  return g_active ? (void *)hooked_glDrawArrays : nullptr;
}
void *getBindTexHook() {
  return g_active ? (void *)hooked_glBindTexture : nullptr;
}
} // namespace boost_drawcall_sort
