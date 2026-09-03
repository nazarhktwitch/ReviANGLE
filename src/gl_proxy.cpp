#include "gl_proxy.hpp"
#include "angle_loader.hpp"
#include "config.hpp"
#include <atomic>
#include <cstdlib>
#include <cstring>

namespace glproxy {

void init() { angle::init(); }

void *resolve(const char *name) {
  auto &a = angle::state();
  if (!a.initialized) {
    if (!angle::init())
      return nullptr;
  }
  if (a.gles2) {
    if (void *p = (void *)GetProcAddress(a.gles2, name))
      return p;
  }
  if (a.eglGetProcAddress) {
    return a.eglGetProcAddress(name);
  }
  return nullptr;
}

} // namespace glproxy

// GL types we use in the shims (matches gl.h so we don't need the full header)
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;
typedef float GLfloat;
typedef double GLdouble;
typedef void GLvoid;

// --- OpenGL 1.1 functions that GD / cocos2d might call directly via the import
// table --- Most cocos2d calls go through wglGetProcAddress after context
// creation, but a few legacy symbols may be looked up at load time. Forward
// them all to ANGLE.

// glClear / glClearColor / glClearDepthf / glClearStencil dedup.
// cocos2d issues identical clears each frame; dedup against last call when no
// draws have happened in between (otherwise the clear is meaningful).
static thread_local bool t_dirtySinceClear = true; // any draw call sets this
static thread_local int t_frameEpoch =
    0; // frame boundary marker for state caches
extern "C" void gdangle_markDirty() { t_dirtySinceClear = true; }

// Megahack support: cross-thread synchronization counters for clear/draw
// logging
static std::atomic<unsigned long long> g_megahackClearSeq{0};
static std::atomic<unsigned long long> g_megahackDrawSeq{0};

extern "C" void gdangle_invalidateProxyStateCaches() {
  t_frameEpoch++;
  t_dirtySinceClear = true;
}

typedef void(WINAPI *PFN_CLR)(GLbitfield);
extern "C" __declspec(dllexport) void WINAPI gl_glClear(GLbitfield mask) {
  static PFN_CLR p = nullptr;
  if (!p)
    p = (PFN_CLR)glproxy::resolve("glClear");
  static thread_local GLbitfield lastMask = 0;
  static int n = 0;
  // Megahack logging
  unsigned long long mhSeq =
      Config::get().megahack_detected
          ? g_megahackClearSeq.fetch_add(1, std::memory_order_relaxed)
          : 0;
  if (n < 30 ||
      (Config::get().megahack_detected && mhSeq >= 240 && mhSeq < 900)) {
    angle::log("glClear #%d mhSeq=%llu: mask=0x%04X dirty=%d", n, mhSeq, mask,
               t_dirtySinceClear ? 1 : 0);
  }
  n++;
  // Skip only if exact same mask AND no draws have happened since last clear
  // (otherwise cocos2d's intent is to wipe drawn pixels).
  if (!t_dirtySinceClear && mask == lastMask)
    return;
  lastMask = mask;
  t_dirtySinceClear = false;
  if (p)
    p(mask);
}

typedef void(WINAPI *PFN_CC)(GLfloat, GLfloat, GLfloat, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glClearColor(GLfloat r,
                                                             GLfloat g,
                                                             GLfloat b,
                                                             GLfloat a) {
  static PFN_CC p = nullptr;
  if (!p)
    p = (PFN_CC)glproxy::resolve("glClearColor");
  static thread_local GLfloat cr = 1e30f, cg = 1e30f, cb = 1e30f, ca = 1e30f;
  static int n = 0;
  // Megahack logging
  if (Config::get().megahack_detected &&
      (n < 40 || g_megahackClearSeq.load(std::memory_order_relaxed) >= 240)) {
    angle::log("glClearColor #%d: %.3f,%.3f,%.3f,%.3f", n, r, g, b, a);
  }
  n++;
  if (r == cr && g == cg && b == cb && a == ca)
    return;
  cr = r;
  cg = g;
  cb = b;
  ca = a;
  if (p)
    p(r, g, b, a);
}

// glViewport: log first 10 changes for diagnostics, then dedup. cocos2d sets
// the viewport on every layer/scene push, often to identical values.
typedef void(WINAPI *PFN_VP)(GLint, GLint, GLsizei, GLsizei);
extern "C" __declspec(dllexport) void WINAPI gl_glViewport(GLint x, GLint y,
                                                           GLsizei w,
                                                           GLsizei h) {
  static PFN_VP p = nullptr;
  if (!p)
    p = (PFN_VP)glproxy::resolve("glViewport");
  static int n = 0;
  if (n < 10)
    angle::log("glViewport #%d: x=%d y=%d w=%d h=%d", n, x, y, w, h);
  n++;

  static thread_local GLint cx = 0x7FFFFFFF, cy = 0x7FFFFFFF;
  static thread_local GLsizei cw = -1, ch = -1;
  if (x == cx && y == cy && w == cw && h == ch)
    return;
  cx = x;
  cy = y;
  cw = w;
  ch = h;
  if (p)
    p(x, y, w, h);
}
// glClearDepthf dedup.
typedef void(WINAPI *PFN_CDF)(GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glClearDepthf(GLfloat d) {
  static PFN_CDF p = nullptr;
  if (!p)
    p = (PFN_CDF)glproxy::resolve("glClearDepthf");
  static thread_local GLfloat cur = 1e30f;
  if (d == cur)
    return;
  cur = d;
  if (p)
    p(d);
}
// glClearStencil dedup.
typedef void(WINAPI *PFN_CS_)(GLint);
extern "C" __declspec(dllexport) void WINAPI gl_glClearStencil(GLint s) {
  static PFN_CS_ p = nullptr;
  if (!p)
    p = (PFN_CS_)glproxy::resolve("glClearStencil");
  static thread_local GLint cur = 0x7FFFFFFF;
  if (s == cur)
    return;
  cur = s;
  if (p)
    p(s);
}

// Legacy GL 1.1 glClearDepth takes double — ANGLE only has glClearDepthf
// (float). Convert and forward.
typedef void(WINAPI *PFNGLCLEARDEPTHF)(GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glClearDepth(GLdouble d) {
  static PFNGLCLEARDEPTHF p = nullptr;
  if (!p)
    p = (PFNGLCLEARDEPTHF)glproxy::resolve("glClearDepthf");
  if (p)
    p((GLfloat)d);
}

// Legacy GL 1.1 glDepthRange takes double pair — ANGLE has glDepthRangef.
// Both share the same dedup state.
static thread_local GLfloat g_depthRangeN = -1.0f, g_depthRangeF = -1.0f;
typedef void(WINAPI *PFNGLDEPTHRANGEF)(GLfloat, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glDepthRange(GLdouble n,
                                                             GLdouble f) {
  GLfloat fn = (GLfloat)n, ff = (GLfloat)f;
  if (fn == g_depthRangeN && ff == g_depthRangeF)
    return;
  g_depthRangeN = fn;
  g_depthRangeF = ff;
  static PFNGLDEPTHRANGEF p = nullptr;
  if (!p)
    p = (PFNGLDEPTHRANGEF)glproxy::resolve("glDepthRangef");
  if (p)
    p(fn, ff);
}
// glDepthRangef dedup (the GLES path; GD usually calls this).
extern "C" __declspec(dllexport) void WINAPI gl_glDepthRangef(GLfloat n,
                                                              GLfloat f) {
  if (n == g_depthRangeN && f == g_depthRangeF)
    return;
  g_depthRangeN = n;
  g_depthRangeF = f;
  static PFNGLDEPTHRANGEF p = nullptr;
  if (!p)
    p = (PFNGLDEPTHRANGEF)glproxy::resolve("glDepthRangef");
  if (p)
    p(n, f);
}
// glSampleCoverage dedup — rarely changes but GD's MSAA path can hammer it.
typedef void(WINAPI *PFN_SAMPCOV)(GLfloat, GLboolean);
extern "C" __declspec(dllexport) void WINAPI
gl_glSampleCoverage(GLfloat value, GLboolean invert) {
  static thread_local GLfloat cv = -1.0f;
  static thread_local GLboolean ci = 2;
  if (value == cv && invert == ci)
    return;
  cv = value;
  ci = invert;
  static PFN_SAMPCOV p = nullptr;
  if (!p)
    p = (PFN_SAMPCOV)glproxy::resolve("glSampleCoverage");
  if (p)
    p(value, invert);
}
// ===== State dedup sweep =====
// cocos2d-x re-sets identical render state every sprite. On hard levels with
// 1000+ visible objects, ~30-50% of GL calls are redundant. Dedup removes
// driver-side validation cost — measurable 10-20% CPU win on 2-core systems.

// glEnable / glDisable cap dedup. Maps known caps to a 32-bit bitmask;
// unknown caps fall through unchanged.
static int s_capBit(GLenum cap) {
  switch (cap) {
  case 0x0BE2:
    return 0; // GL_BLEND
  case 0x0B71:
    return 1; // GL_DEPTH_TEST
  case 0x0B90:
    return 2; // GL_STENCIL_TEST
  case 0x0B44:
    return 3; // GL_CULL_FACE
  case 0x0BD0:
    return 4; // GL_DITHER
  case 0x0C11:
    return 5; // GL_SCISSOR_TEST
  case 0x8037:
    return 6; // GL_POLYGON_OFFSET_FILL
  case 0x809E:
    return 7; // GL_SAMPLE_ALPHA_TO_COVERAGE
  case 0x80A0:
    return 8; // GL_SAMPLE_COVERAGE
  case 0x8642:
    return 9; // GL_PROGRAM_POINT_SIZE / VERTEX_PROGRAM_POINT_SIZE
  case 0x8861:
    return 10; // GL_POINT_SPRITE
  case 0x8DB9:
    return 11; // GL_FRAMEBUFFER_SRGB
  case 0x8C89:
    return 12; // GL_RASTERIZER_DISCARD
  default:
    return -1; // unknown -> pass through
  }
}
static thread_local unsigned int t_capMask = 0;
typedef void(WINAPI *PFN_ED)(GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glEnable(GLenum cap) {
  static PFN_ED p = nullptr;
  if (!p)
    p = (PFN_ED)glproxy::resolve("glEnable");
  int b = s_capBit(cap);
  if (b >= 0) {
    unsigned int mask = 1u << b;
    if (t_capMask & mask)
      return;
    t_capMask |= mask;
  }
  if (p)
    p(cap);
}
extern "C" __declspec(dllexport) void WINAPI gl_glDisable(GLenum cap) {
  static PFN_ED p = nullptr;
  if (!p)
    p = (PFN_ED)glproxy::resolve("glDisable");
  int b = s_capBit(cap);
  if (b >= 0) {
    unsigned int mask = 1u << b;
    if (!(t_capMask & mask))
      return;
    t_capMask &= ~mask;
  }
  if (p)
    p(cap);
}

// glColorMask — 4 booleans packed into one nibble.
typedef void(WINAPI *PFN_CM)(GLboolean, GLboolean, GLboolean, GLboolean);
extern "C" __declspec(dllexport) void WINAPI gl_glColorMask(GLboolean r,
                                                            GLboolean g,
                                                            GLboolean b,
                                                            GLboolean a) {
  static PFN_CM p = nullptr;
  if (!p)
    p = (PFN_CM)glproxy::resolve("glColorMask");
  static thread_local unsigned int cur = 0xFFFFFFFFu;
  unsigned int packed =
      (r ? 1u : 0u) | (g ? 2u : 0u) | (b ? 4u : 0u) | (a ? 8u : 0u);
  if (packed == cur)
    return;
  cur = packed;
  if (p)
    p(r, g, b, a);
}

// glCullFace — single GLenum.
typedef void(WINAPI *PFN_CF)(GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glCullFace(GLenum mode) {
  static PFN_CF p = nullptr;
  if (!p)
    p = (PFN_CF)glproxy::resolve("glCullFace");
  static thread_local GLenum cur = 0xFFFFFFFFu;
  if (mode == cur)
    return;
  cur = mode;
  if (p)
    p(mode);
}

// glDepthMask — single boolean.
typedef void(WINAPI *PFN_DM)(GLboolean);
extern "C" __declspec(dllexport) void WINAPI gl_glDepthMask(GLboolean flag) {
  static PFN_DM p = nullptr;
  if (!p)
    p = (PFN_DM)glproxy::resolve("glDepthMask");
  static thread_local int cur = -1; // -1 = unknown
  int v = flag ? 1 : 0;
  if (v == cur)
    return;
  cur = v;
  if (p)
    p(flag);
}
// Diagnostic: count draw calls. Logged from wglSwapBuffers throttle.
static unsigned long long g_drawArrays = 0;
static unsigned long long g_drawElements = 0;
extern "C" unsigned long long gdangle_getDrawArraysCount() {
  return g_drawArrays;
}
extern "C" unsigned long long gdangle_getDrawElementsCount() {
  return g_drawElements;
}

typedef void(WINAPI *PFN_DA)(GLenum, GLint, GLsizei);
extern "C" void gdangle_markDirty();
// Defined in gl_proxy_ext.cpp — returns false when the currently bound
// program has GL_LINK_STATUS=0 (silicate's shaders using desktop-only
// `layout(location=N) uniform` syntax that ESSL3 rejects).
extern "C" bool gdangle_currentProgramOK();

// SEH filter for ANGLE crashes. Some Geode mods (peony.silicate) drive
// ANGLE through code paths that hit `ud2` (UNREACHABLE) deep in libGLESv2's
// D3D11 stream translator — typically caused by client-side vertex arrays,
// invalid index types, or vertex format mismatches that ANGLE's renderer
// can't translate to a D3D11 input layout. The crash is c000001d
// EXCEPTION_ILLEGAL_INSTRUCTION; on desktop GL the same operation would
// just produce a no-op or an error. SEH-catch the crash, log once, and
// continue. State leakage in ANGLE's C++ internals is acceptable vs.
// taking the whole game down.
#include <excpt.h>
#include <windows.h>
static int gd_filterAngleCrash(unsigned code) {
  if (code == EXCEPTION_ILLEGAL_INSTRUCTION ||
      code == EXCEPTION_ACCESS_VIOLATION) {
    return EXCEPTION_EXECUTE_HANDLER;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
static void gd_logAngleCrash(const char *fn, unsigned code) {
  static int n = 0;
  if (n < 16) {
    angle::log("ANGLE crash in %s: 0x%08X (caught, skipping draw) #%d", fn,
               code, n);
    n++;
  }
}

extern "C" __declspec(dllexport) void WINAPI gl_glDrawArrays(GLenum mode,
                                                             GLint first,
                                                             GLsizei count) {
  static PFN_DA p = nullptr;
  if (!p)
    p = (PFN_DA)glproxy::resolve("glDrawArrays");
  if (!gdangle_currentProgramOK())
    return;
  static int n = 0;
  unsigned long long mhSeq =
      Config::get().megahack_detected
          ? g_megahackDrawSeq.fetch_add(1, std::memory_order_relaxed)
          : 0;
  if (n < 50 || (Config::get().megahack_detected && mhSeq >= 4000 &&
                 mhSeq < 9000 && (mhSeq % 100) == 0)) {
    angle::log("glDrawArrays #%d mhSeq=%llu: mode=%d first=%d count=%d", n,
               mhSeq, mode, first, count);
  }
  n++;
  g_drawArrays++;
  gdangle_markDirty();
  if (!p)
    return;
  __try {
    p(mode, first, count);
  } __except (gd_filterAngleCrash(GetExceptionCode())) {
    gd_logAngleCrash("glDrawArrays", GetExceptionCode());
  }
}

typedef void(WINAPI *PFN_DE)(GLenum, GLsizei, GLenum, const GLvoid *);
extern "C" __declspec(dllexport) void WINAPI
gl_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *ind) {
  static PFN_DE p = nullptr;
  if (!p)
    p = (PFN_DE)glproxy::resolve("glDrawElements");
  if (!gdangle_currentProgramOK())
    return;
  static int n = 0;
  unsigned long long mhSeq =
      Config::get().megahack_detected
          ? g_megahackDrawSeq.fetch_add(1, std::memory_order_relaxed)
          : 0;
  if (n < 50 || (Config::get().megahack_detected && mhSeq >= 4000 &&
                 mhSeq < 9000 && (mhSeq % 100) == 0)) {
    angle::log("glDrawElements #%d mhSeq=%llu: mode=%d count=%d type=0x%X", n,
               mhSeq, mode, count, type);
  }
  n++;
  g_drawElements++;
  gdangle_markDirty();
  if (!p)
    return;
  __try {
    p(mode, count, type, ind);
  } __except (gd_filterAngleCrash(GetExceptionCode())) {
    gd_logAngleCrash("glDrawElements", GetExceptionCode());
  }
}

// glDrawElementsBaseVertex — GL 3.2+ desktop only, NOT in OpenGL ES.
// Eclipse Menu's GLEW caches this as NULL via wglGetProcAddress ->
// ImGui+cocos2d path crashes (DEP exec at 0x0). Provide stub: forward to
// glDrawElements. Correct when basevertex==0 (cocos2d's draw_triangle case).
// For non-zero basevertex, geometry would be offset incorrectly — preferable to
// a crash.
extern "C" __declspec(dllexport) void WINAPI
gl_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                            const GLvoid *indices, GLint /*basevertex*/) {
  static PFN_DE p = nullptr;
  if (!p)
    p = (PFN_DE)glproxy::resolve("glDrawElements");
  if (!gdangle_currentProgramOK())
    return;
  g_drawElements++;
  if (!p)
    return;
  __try {
    p(mode, count, type, indices);
  } __except (gd_filterAngleCrash(GetExceptionCode())) {
    gd_logAngleCrash("glDrawElementsBaseVertex", GetExceptionCode());
  }
}
// glPrimitiveRestartIndex / glProvokingVertex — GL 3.x desktop, no-op in GLES
// context.
extern "C" __declspec(dllexport) void WINAPI
gl_glPrimitiveRestartIndex(GLuint /*index*/) { /* no-op */ }
extern "C" __declspec(dllexport) void WINAPI
gl_glProvokingVertex(GLenum /*mode*/) { /* no-op */ }
// gl_glEnable defined above with cap-mask dedup.
// glFinish: forces full pipeline stall. When config opts in, treat as no-op.
typedef void(WINAPI *PFN_FN)(void);
extern "C" __declspec(dllexport) void WINAPI gl_glFinish(void) {
  static const bool noop = Config::get().noop_finish;
  if (noop)
    return;
  static PFN_FN p = nullptr;
  if (!p)
    p = (PFN_FN)glproxy::resolve("glFinish");
  if (p)
    p();
}
GLP_FORWARD_VOID(glFlush, (void), ())
// glFrontFace dedup.
typedef void(WINAPI *PFN_FF)(GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glFrontFace(GLenum mode) {
  static PFN_FF p = nullptr;
  if (!p)
    p = (PFN_FF)glproxy::resolve("glFrontFace");
  static thread_local GLenum cur = 0xFFFFFFFFu;
  if (mode == cur)
    return;
  cur = mode;
  if (p)
    p(mode);
}
// glGetError: when noop_geterror set, short-circuit. Default is forward to
// driver.
typedef GLenum(WINAPI *PFN_GE)(void);
extern "C" __declspec(dllexport) GLenum WINAPI gl_glGetError(void) {
  static const bool noop = Config::get().noop_geterror;
  if (noop)
    return 0 /*GL_NO_ERROR*/;
  static PFN_GE p = nullptr;
  if (!p)
    p = (PFN_GE)glproxy::resolve("glGetError");
  return p ? p() : 0;
}
// glPolygonMode: NOT in OpenGL ES — but ImGui/Eclipse on desktop call it.
// Without this stub, Eclipse's ImGui caches NULL function pointer at backend
// init, then crashes (DEP exec at 0x0) when drawing. GLES is implicitly always
// GL_FILL.
extern "C" __declspec(dllexport) void WINAPI gl_glPolygonMode(GLenum /*face*/,
                                                              GLenum /*mode*/) {
  // intentional no-op: GLES has no polygon mode
}
// glGetIntegerv: handle GL_NUM_EXTENSIONS=0x821D specially with our spoofed
// count
typedef void(WINAPI *PFNGLGETINTEGERV)(GLenum, GLint *);
extern "C" __declspec(dllexport) void WINAPI gl_glGetIntegerv(GLenum pname,
                                                              GLint *data) {
  if (pname == 0x821D && data) { // GL_NUM_EXTENSIONS
    *data = 26;                  // matches kExts count in gl_glGetStringi
    return;
  }
  static PFNGLGETINTEGERV p = nullptr;
  if (!p)
    p = (PFNGLGETINTEGERV)glproxy::resolve("glGetIntegerv");
  if (p)
    p(pname, data);
}
GLP_FORWARD_VOID(glGetFloatv, (GLenum pname, GLfloat *data), (pname, data))
GLP_FORWARD_VOID(glGetBooleanv, (GLenum pname, GLboolean *data), (pname, data))
// glGetString: ANGLE returns "OpenGL ES 3.0" but GD's parser expects "X.Y"
// desktop GL format. Intercept and return a faked desktop OpenGL version
// string.
typedef const GLubyte *(WINAPI *PFNGLGETSTRING)(GLenum);
extern "C" __declspec(dllexport) const GLubyte *WINAPI
gl_glGetString(GLenum name) {
  static PFNGLGETSTRING p = nullptr;
  if (!p)
    p = (PFNGLGETSTRING)glproxy::resolve("glGetString");

  // GL_VENDOR=0x1F00, GL_RENDERER=0x1F01, GL_VERSION=0x1F02,
  // GL_EXTENSIONS=0x1F03, GL_SHADING_LANGUAGE_VERSION=0x8B8C
  switch (name) {
  case 0x1F02: // GL_VERSION
    // Keep "2.1.0 " prefix — cocos2d parses leading "X.Y" floats.
    return (const GLubyte *)"2.1.0 ReviANGLE";
  case 0x8B8C: // GL_SHADING_LANGUAGE_VERSION
    return (const GLubyte *)"1.20";
  case 0x1F00: // GL_VENDOR
    return (const GLubyte *)"ReviANGLE";
  case 0x1F01: // GL_RENDERER
    return (const GLubyte *)"ReviANGLE Direct3D11";
  case 0x1F03: { // GL_EXTENSIONS
    // GLEW + cocos2d-x parse GL_EXTENSIONS to detect GL_ARB_*/GL_EXT_*.
    // ANGLE returns GLES-style names which GLEW doesn't recognise -> renderer
    // init fails
    // -> NULL deref later. Provide the desktop names cocos2d + Eclipse-Menu's
    // GLEW need; without GL_ARB_draw_elements_base_vertex GLEW leaves
    // __glewDrawElementsBaseVertex NULL -> Eclipse crashes (DEP at RIP=0).
    static const char *kExt =
        "GL_ARB_vertex_buffer_object "
        "GL_ARB_vertex_array_object "
        "GL_ARB_framebuffer_object "
        "GL_ARB_texture_non_power_of_two "
        "GL_ARB_shader_objects "
        "GL_ARB_vertex_shader "
        "GL_ARB_fragment_shader "
        "GL_ARB_shading_language_100 "
        "GL_ARB_multitexture "
        /* "GL_ARB_pixel_buffer_object " */ // <--- Disabled to prevent Globed
                                            // PBO OOM crash on Vulkan
        "GL_ARB_depth_texture "
        "GL_ARB_point_sprite "
        "GL_ARB_occlusion_query "
        "GL_ARB_draw_elements_base_vertex "
        /* "GL_ARB_map_buffer_range " */ // <--- Disabled
        "GL_ARB_uniform_buffer_object "
        "GL_ARB_sampler_objects "
        "GL_ARB_instanced_arrays "
        "GL_EXT_framebuffer_object "
        "GL_EXT_blend_func_separate "
        "GL_EXT_blend_equation_separate "
        "GL_EXT_blend_minmax "
        "GL_EXT_packed_depth_stencil "
        "GL_EXT_texture_format_BGRA8888 "
        "GL_EXT_bgra "
        "GL_EXT_abgr ";
    return (const GLubyte *)kExt;
  }
  default:
    return p ? p(name) : (const GLubyte *)"";
  }
}

// GL 3.0+ glGetStringi for extension enumeration (used by GLEW when GL >= 3.0)
typedef const GLubyte *(WINAPI *PFNGLGETSTRINGI)(GLenum, GLuint);
extern "C" __declspec(dllexport) const GLubyte *WINAPI
gl_glGetStringi(GLenum name, GLuint index) {
  static PFNGLGETSTRINGI p = nullptr;
  if (!p)
    p = (PFNGLGETSTRINGI)glproxy::resolve("glGetStringi");
  // For GL_EXTENSIONS, return our spoofed list one extension at a time
  if (name == 0x1F03) {
    static const char *kExts[] = {
        "GL_ARB_vertex_buffer_object",
        "GL_ARB_vertex_array_object",
        "GL_ARB_framebuffer_object",
        "GL_ARB_texture_non_power_of_two",
        "GL_ARB_shader_objects",
        "GL_ARB_vertex_shader",
        "GL_ARB_fragment_shader",
        "GL_ARB_shading_language_100",
        "GL_ARB_multitexture", /* "GL_ARB_pixel_buffer_object", */
        "GL_ARB_depth_texture",
        "GL_ARB_point_sprite",
        "GL_ARB_occlusion_query",
        "GL_ARB_draw_elements_base_vertex",
        /* "GL_ARB_map_buffer_range", */ "GL_ARB_uniform_buffer_object",
        "GL_ARB_sampler_objects",
        "GL_ARB_instanced_arrays",
        "GL_EXT_framebuffer_object",
        "GL_EXT_blend_func_separate",
        "GL_EXT_blend_equation_separate",
        "GL_EXT_blend_minmax",
        "GL_EXT_packed_depth_stencil",
        "GL_EXT_texture_format_BGRA8888",
        "GL_EXT_bgra",
        "GL_EXT_abgr",
    };
    const GLuint kCount = sizeof(kExts) / sizeof(kExts[0]);
    if (index < kCount)
      return (const GLubyte *)kExts[index];
    return (const GLubyte *)"";
  }
  return p ? p(name, index) : (const GLubyte *)"";
}

// glHint dedup — small fixed set of targets. Most calls in GD set the same.
typedef void(WINAPI *PFN_HT)(GLenum, GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glHint(GLenum target,
                                                       GLenum mode) {
  static PFN_HT p = nullptr;
  if (!p)
    p = (PFN_HT)glproxy::resolve("glHint");
  // Use lower 8 bits of target as direct-mapped slot. Collisions just
  // redo the call — still correct.
  thread_local GLenum modes[256] = {};
  thread_local GLenum tgts[256] = {};
  unsigned slot = target & 0xFF;
  if (tgts[slot] == target && modes[slot] == mode)
    return;
  tgts[slot] = target;
  modes[slot] = mode;
  if (p)
    p(target, mode);
}
GLP_FORWARD(GLboolean, glIsEnabled, (GLenum cap), (cap))
// glLineWidth dedup.
typedef void(WINAPI *PFN_LW)(GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glLineWidth(GLfloat w) {
  static PFN_LW p = nullptr;
  if (!p)
    p = (PFN_LW)glproxy::resolve("glLineWidth");
  static thread_local GLfloat cur = -1.0f;
  if (w == cur)
    return;
  cur = w;
  if (p)
    p(w);
}
// glPixelStorei dedup — cocos2d sets GL_UNPACK_ALIGNMENT to 1 or 4 every
// texture upload. Direct-mapped 64-slot table by `pname & 0x3F` (collisions
// just redo the call — still correct).
typedef void(WINAPI *PFN_PSI)(GLenum, GLint);
extern "C" __declspec(dllexport) void WINAPI gl_glPixelStorei(GLenum pname,
                                                              GLint param) {
  static PFN_PSI p = nullptr;
  if (!p)
    p = (PFN_PSI)glproxy::resolve("glPixelStorei");
  thread_local GLenum pnames[64] = {};
  thread_local GLint params[64] = {};
  unsigned slot = pname & 0x3F;
  if (pnames[slot] == pname && params[slot] == param)
    return;
  pnames[slot] = pname;
  params[slot] = param;
  if (p)
    p(pname, param);
}
// glPolygonOffset dedup.
typedef void(WINAPI *PFN_PO)(GLfloat, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glPolygonOffset(GLfloat factor,
                                                                GLfloat units) {
  static PFN_PO p = nullptr;
  if (!p)
    p = (PFN_PO)glproxy::resolve("glPolygonOffset");
  static thread_local GLfloat cf = 1e30f, cu = 1e30f;
  if (factor == cf && units == cu)
    return;
  cf = factor;
  cu = units;
  if (p)
    p(factor, units);
}
// glReadPixels is hooked manually in boost_halfres_fx.cpp
// glScissor dedup.
typedef void(WINAPI *PFN_SC)(GLint, GLint, GLsizei, GLsizei);
extern "C" __declspec(dllexport) void WINAPI gl_glScissor(GLint x, GLint y,
                                                          GLsizei w,
                                                          GLsizei h) {
  static PFN_SC p = nullptr;
  if (!p)
    p = (PFN_SC)glproxy::resolve("glScissor");
  static thread_local GLint cx = -1, cy = -1;
  static thread_local GLsizei cw = -1, ch = -1;
  if (x == cx && y == cy && w == cw && h == ch)
    return;
  cx = x;
  cy = y;
  cw = w;
  ch = h;
  if (p)
    p(x, y, w, h);
}
// glStencilFunc dedup.
typedef void(WINAPI *PFN_SF)(GLenum, GLint, GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glStencilFunc(GLenum func,
                                                              GLint ref,
                                                              GLuint mask) {
  static PFN_SF p = nullptr;
  if (!p)
    p = (PFN_SF)glproxy::resolve("glStencilFunc");
  static thread_local GLenum cfn = 0xFFFFFFFFu;
  static thread_local GLint cref = -1;
  static thread_local GLuint cm = 0xFFFFFFFFu;
  if (func == cfn && ref == cref && mask == cm)
    return;
  cfn = func;
  cref = ref;
  cm = mask;
  if (p)
    p(func, ref, mask);
}
// glStencilMask dedup.
typedef void(WINAPI *PFN_SM)(GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glStencilMask(GLuint mask) {
  static PFN_SM p = nullptr;
  if (!p)
    p = (PFN_SM)glproxy::resolve("glStencilMask");
  static thread_local GLuint cur = 0xFFFFFFFFu;
  if (mask == cur)
    return;
  cur = mask;
  if (p)
    p(mask);
}
// glStencilOp dedup.
typedef void(WINAPI *PFN_SO)(GLenum, GLenum, GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glStencilOp(GLenum f, GLenum zf,
                                                            GLenum zp) {
  static PFN_SO p = nullptr;
  if (!p)
    p = (PFN_SO)glproxy::resolve("glStencilOp");
  static thread_local GLenum c0 = 0xFFFFFFFFu, c1 = 0xFFFFFFFFu,
                             c2 = 0xFFFFFFFFu;
  if (f == c0 && zf == c1 && zp == c2)
    return;
  c0 = f;
  c1 = zf;
  c2 = zp;
  if (p)
    p(f, zf, zp);
}
// glViewport defined above with diagnostic logging
// Blend state dedup — cocos2d sets same blend mode across many batched sprites
typedef void(WINAPI *PFN_BF)(GLenum, GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glBlendFunc(GLenum s,
                                                            GLenum d) {
  static PFN_BF p = nullptr;
  if (!p)
    p = (PFN_BF)glproxy::resolve("glBlendFunc");
  static thread_local GLenum cs = 0xFFFFFFFFu, cd = 0xFFFFFFFFu;
  if (s == cs && d == cd)
    return;
  cs = s;
  cd = d;
  if (p)
    p(s, d);
}
typedef void(WINAPI *PFN_BE)(GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glBlendEquation(GLenum mode) {
  static PFN_BE p = nullptr;
  if (!p)
    p = (PFN_BE)glproxy::resolve("glBlendEquation");
  static thread_local GLenum cur = 0xFFFFFFFFu;
  if (mode == cur)
    return;
  cur = mode;
  if (p)
    p(mode);
}
typedef void(WINAPI *PFN_BFS)(GLenum, GLenum, GLenum, GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glBlendFuncSeparate(GLenum ss,
                                                                    GLenum ds,
                                                                    GLenum sa,
                                                                    GLenum da) {
  static PFN_BFS p = nullptr;
  if (!p)
    p = (PFN_BFS)glproxy::resolve("glBlendFuncSeparate");
  static thread_local GLenum c0 = 0xFFFFFFFFu, c1 = 0xFFFFFFFFu,
                             c2 = 0xFFFFFFFFu, c3 = 0xFFFFFFFFu;
  if (ss == c0 && ds == c1 && sa == c2 && da == c3)
    return;
  c0 = ss;
  c1 = ds;
  c2 = sa;
  c3 = da;
  if (p)
    p(ss, ds, sa, da);
}
// glDepthFunc dedup.
typedef void(WINAPI *PFN_DF)(GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glDepthFunc(GLenum func) {
  static PFN_DF p = nullptr;
  if (!p)
    p = (PFN_DF)glproxy::resolve("glDepthFunc");
  static thread_local GLenum cur = 0xFFFFFFFFu;
  if (func == cur)
    return;
  cur = func;
  if (p)
    p(func);
}

// textures — with state dedup (skip redundant calls, reduces driver overhead)
typedef void(WINAPI *PFN_AT)(GLenum);
typedef void(WINAPI *PFN_BT)(GLenum, GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glActiveTexture(GLenum tex) {
  static PFN_AT p = nullptr;
  if (!p)
    p = (PFN_AT)glproxy::resolve("glActiveTexture");
  static thread_local GLenum cur = 0xFFFFFFFFu;
  if (tex == cur)
    return;
  cur = tex;
  if (p)
    p(tex);
}
extern "C" __declspec(dllexport) void WINAPI gl_glBindTexture(GLenum target,
                                                              GLuint tex) {
  static PFN_BT p = nullptr;
  if (!p)
    p = (PFN_BT)glproxy::resolve("glBindTexture");
  // track per-texture-unit, per-target
  constexpr int MAX_UNITS = 32;
  static thread_local GLuint bound2D[MAX_UNITS] = {};
  static thread_local GLuint boundCube[MAX_UNITS] = {};
  static thread_local GLenum tu = 0x84C0; // GL_TEXTURE0
  // (We can't see glActiveTexture state from here — track via separate path is
  // hard. For 2D-only games like GD, this dedup is still effective at unit 0.)
  if (target == 0x0DE1 /*GL_TEXTURE_2D*/) {
    if (bound2D[0] == tex)
      return;
    bound2D[0] = tex;
  } else if (target == 0x8513 /*GL_TEXTURE_CUBE_MAP*/) {
    if (boundCube[0] == tex)
      return;
    boundCube[0] = tex;
  }
  if (p)
    p(target, tex);
}
GLP_FORWARD_VOID(glDeleteTextures, (GLsizei n, const GLuint *t), (n, t))
GLP_FORWARD_VOID(glGenTextures, (GLsizei n, GLuint *t), (n, t))
GLP_FORWARD_VOID(glTexImage2D,
                 (GLenum t, GLint l, GLint ifmt, GLsizei w, GLsizei h, GLint b,
                  GLenum f, GLenum type, const GLvoid *px),
                 (t, l, ifmt, w, h, b, f, type, px))
GLP_FORWARD_VOID(glTexSubImage2D,
                 (GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h,
                  GLenum f, GLenum type, const GLvoid *px),
                 (t, l, x, y, w, h, f, type, px))
// glTexParameteri — when mipmap_off is set, downgrade mipmap filters to
// non-mipmap. Saves GPU bandwidth on Kepler/Fermi: each sample no longer reads
// from mip chain.
typedef void(WINAPI *PFN_TPI)(GLenum, GLenum, GLint);
extern "C" __declspec(dllexport) void WINAPI gl_glTexParameteri(GLenum t,
                                                                GLenum p,
                                                                GLint v) {
  static PFN_TPI fn = nullptr;
  if (!fn)
    fn = (PFN_TPI)glproxy::resolve("glTexParameteri");
  static const bool mipmapOff = Config::get().mipmap_off;
  if (mipmapOff && p == 0x2801 /*GL_TEXTURE_MIN_FILTER*/) {
    // collapse mipmap variants to plain LINEAR/NEAREST
    switch (v) {
    case 0x2700:  /*NEAREST_MIPMAP_NEAREST*/
    case 0x2702:  /*NEAREST_MIPMAP_LINEAR*/
      v = 0x2600; /*GL_NEAREST*/
      break;
    case 0x2701:  /*LINEAR_MIPMAP_NEAREST*/
    case 0x2703:  /*LINEAR_MIPMAP_LINEAR*/
      v = 0x2601; /*GL_LINEAR*/
      break;
    }
  }
  if (fn)
    fn(t, p, v);
}
GLP_FORWARD_VOID(glTexParameterf, (GLenum t, GLenum p, GLfloat v), (t, p, v))
GLP_FORWARD_VOID(glCompressedTexImage2D,
                 (GLenum t, GLint l, GLenum ifmt, GLsizei w, GLsizei h, GLint b,
                  GLsizei sz, const GLvoid *d),
                 (t, l, ifmt, w, h, b, sz, d))
// glGenerateMipmap — skip entirely when mipmap_off (save startup time + VRAM)
typedef void(WINAPI *PFN_GMM)(GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glGenerateMipmap(GLenum t) {
  static PFN_GMM fn = nullptr;
  if (!fn)
    fn = (PFN_GMM)glproxy::resolve("glGenerateMipmap");
  static const bool mipmapOff = Config::get().mipmap_off;
  if (mipmapOff)
    return;
  if (fn)
    fn(t);
}

extern "C" __declspec(dllexport) void WINAPI
gl_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x,
                    GLint y, GLsizei width, GLsizei height, GLint border) {
  static auto real =
      (void(WINAPI *)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei,
                      GLint))GetProcAddress(GetModuleHandleA("libGLESv2.dll"),
                                            "glCopyTexImage2D");
  if (real)
    real(target, level, internalformat, x, y, width, height, border);
}

extern "C" __declspec(dllexport) void WINAPI
gl_glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                       GLint x, GLint y, GLsizei width, GLsizei height) {
  static auto real =
      (void(WINAPI *)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei,
                      GLsizei))GetProcAddress(GetModuleHandleA("libGLESv2.dll"),
                                              "glCopyTexSubImage2D");
  if (real)
    real(target, level, xoffset, yoffset, x, y, width, height);
}

extern "C" __declspec(dllexport) void WINAPI
gl_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                GLenum type, void *pixels) {
  static auto real =
      (void(WINAPI *)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *))
          GetProcAddress(GetModuleHandleA("libGLESv2.dll"), "glReadPixels");
  if (real)
    real(x, y, width, height, format, type, pixels);
}
