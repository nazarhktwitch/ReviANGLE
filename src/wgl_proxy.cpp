#include "wgl_proxy.hpp"
#include "angle_loader.hpp"
#include "config.hpp"
#include "wgl_proxy.hpp"
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>


// called once after first successful MakeCurrent
extern void gdangle_postGLInit();
static bool g_glInitDone = false;

// forward decls at global scope (extern "C" linkage cannot be in function
// bodies)
extern "C" bool gdangle_shouldSkipPresent();
extern "C" void gdangle_invalidateAllStateCaches();
extern "C" void gdangle_invalidateProxyStateCaches();

extern "C" unsigned long long gdangle_getDrawArraysCount();
extern "C" unsigned long long gdangle_getDrawElementsCount();

// Frame pacing - declared in boost_frame_pacing.cpp
namespace boost_frame_pacing {
void prePresent();
bool isActive();
int getTargetFps();
} // namespace boost_frame_pacing
// FPS cap unlock - declared in boost_unlock_fps.cpp
namespace boost_unlock_fps {
bool isActive();
}

// EGL constants
constexpr EGLint_t EGL_CONTEXT_CLIENT_VERSION = 0x3098;
constexpr EGLint_t EGL_RED_SIZE = 0x3024;
constexpr EGLint_t EGL_GREEN_SIZE = 0x3023;
constexpr EGLint_t EGL_BLUE_SIZE = 0x3022;
constexpr EGLint_t EGL_ALPHA_SIZE = 0x3021;
constexpr EGLint_t EGL_DEPTH_SIZE = 0x3025;
constexpr EGLint_t EGL_STENCIL_SIZE = 0x3026;
constexpr EGLint_t EGL_SURFACE_TYPE = 0x3033;
constexpr EGLint_t EGL_WINDOW_BIT = 0x0004;
constexpr EGLint_t EGL_RENDERABLE_TYPE = 0x3040;
constexpr EGLint_t EGL_OPENGL_ES2_BIT = 0x0004;
constexpr EGLint_t EGL_NONE = 0x3038;

struct FakeContext {
  EGLContext_t eglCtx = nullptr;
  EGLSurface_t surface = nullptr;
  HDC hdc = nullptr;
  HWND hwnd = nullptr;
};

static std::unordered_map<HGLRC, FakeContext *> g_contexts;
static std::mutex g_mutex;

static thread_local FakeContext *t_current = nullptr;
static thread_local HDC t_currentDC = nullptr;

static EGLConfig_t pickConfig(EGLDisplay_t display) {
  auto &a = angle::state();
  EGLint_t attribs[] = {EGL_RED_SIZE,
                        8,
                        EGL_GREEN_SIZE,
                        8,
                        EGL_BLUE_SIZE,
                        8,
                        EGL_ALPHA_SIZE,
                        8,
                        EGL_DEPTH_SIZE,
                        24,
                        EGL_STENCIL_SIZE,
                        8,
                        EGL_SURFACE_TYPE,
                        EGL_WINDOW_BIT,
                        EGL_RENDERABLE_TYPE,
                        EGL_OPENGL_ES2_BIT,
                        EGL_NONE,
                        EGL_NONE};
  EGLConfig_t config = nullptr;
  EGLint_t num = 0;
  if (!a.eglChooseConfig(display, attribs, &config, 1, &num) || num < 1) {
    return nullptr;
  }
  return config;
}

extern "C" {

HGLRC WINAPI wgl_wglCreateContext(HDC hdc) {
  if (!angle::init())
    return nullptr;
  auto &a = angle::state();

  HWND hwnd = WindowFromDC(hdc);
  if (!hwnd) {
    SetLastError(ERROR_INVALID_WINDOW_HANDLE);
    angle::log("wglCreateContext: no HWND for HDC %p", hdc);
    return nullptr;
  }
  // Diagnose: log window class, title, parent, visibility, size
  {
    char cls[64] = {}, ttl[128] = {};
    GetClassNameA(hwnd, cls, 63);
    GetWindowTextA(hwnd, ttl, 127);
    RECT r = {};
    GetWindowRect(hwnd, &r);
    HWND parent = GetParent(hwnd);
    BOOL vis = IsWindowVisible(hwnd);
    DWORD style = (DWORD)GetWindowLongPtrA(hwnd, GWL_STYLE);
    angle::log("wglCreateContext: hdc=%p hwnd=%p cls='%s' title='%s' parent=%p "
               "vis=%d style=0x%X size=%dx%d",
               hdc, hwnd, cls, ttl, parent, vis, style, r.right - r.left,
               r.bottom - r.top);
  }

  EGLConfig_t cfg = pickConfig(a.display);
  if (!cfg) {
    SetLastError(ERROR_INVALID_PIXEL_FORMAT);
    angle::log("wglCreateContext: eglChooseConfig failed: 0x%x",
               a.eglGetError());
    return nullptr;
  }

  EGLint_t surfAttribs[] = {EGL_NONE, EGL_NONE};
  EGLSurface_t surf =
      a.eglCreateWindowSurface(a.display, cfg, hwnd, surfAttribs);
  if (!surf) {
    SetLastError(ERROR_INVALID_WINDOW_HANDLE);
    angle::log("wglCreateContext: eglCreateWindowSurface failed: 0x%x",
               a.eglGetError());
    return nullptr;
  }

  // EGL_CONTEXT_OPENGL_NO_ERROR_KHR (0x31B3): when set to TRUE, ANGLE skips
  // per-API-call validation (parameter checks, GL state checks). For a
  // draw-call heavy 2D engine like cocos2d this saves significant CPU. ANGLE
  // supports it via EGL_KHR_create_context_no_error (verified present in
  // libGLESv2.dll).
  constexpr EGLint_t EGL_CONTEXT_OPENGL_NO_ERROR_KHR = 0x31B3;
  const bool useNoError = Config::get().gl_no_error;
  EGLint_t ctxAttribsNoError[] = {EGL_CONTEXT_CLIENT_VERSION,
                                  3,
                                  EGL_CONTEXT_OPENGL_NO_ERROR_KHR,
                                  1,
                                  EGL_NONE,
                                  EGL_NONE};
  EGLint_t ctxAttribsBasic[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE,
                                EGL_NONE};
  const EGLint_t *ctxAttribs = useNoError ? ctxAttribsNoError : ctxAttribsBasic;
  EGLContext_t ctx = a.eglCreateContext(a.display, cfg, nullptr, ctxAttribs);
  if (!ctx && useNoError) {
    // Fallback: some ANGLE builds reject the no-error flag; retry without it.
    ctx = a.eglCreateContext(a.display, cfg, nullptr, ctxAttribsBasic);
    if (ctx)
      angle::log("wglCreateContext: NO_ERROR rejected, fell back to validating "
                 "context");
  } else if (ctx && useNoError) {
    angle::log("wglCreateContext: NO_ERROR context active (per-call validation "
               "disabled)");
  }
  if (!ctx) {
    SetLastError(ERROR_INVALID_FUNCTION);
    angle::log("wglCreateContext: eglCreateContext failed: 0x%x",
               a.eglGetError());
    a.eglDestroySurface(a.display, surf);
    return nullptr;
  }

  auto *fc = new FakeContext{ctx, surf, hdc, hwnd};

  std::lock_guard<std::mutex> lock(g_mutex);
  HGLRC fake = (HGLRC)(uintptr_t)(g_contexts.size() + 1);
  // ensure uniqueness
  while (g_contexts.count(fake)) {
    fake = (HGLRC)((uintptr_t)fake + 1);
  }
  g_contexts[fake] = fc;

  angle::log("wglCreateContext -> %p (egl=%p surf=%p)", fake, ctx, surf);
  SetLastError(ERROR_SUCCESS);
  return fake;
}

BOOL WINAPI wgl_wglDeleteContext(HGLRC hglrc) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_contexts.find(hglrc);
  if (it == g_contexts.end())
    return FALSE;

  auto &a = angle::state();
  auto *fc = it->second;
  if (fc->eglCtx)
    a.eglDestroyContext(a.display, fc->eglCtx);
  if (fc->surface)
    a.eglDestroySurface(a.display, fc->surface);
  delete fc;
  g_contexts.erase(it);
  return TRUE;
}

BOOL WINAPI wgl_wglMakeCurrent(HDC hdc, HGLRC hglrc) {
  auto &a = angle::state();

  if (!hglrc) {
    a.eglMakeCurrent(a.display, nullptr, nullptr, nullptr);
    t_current = nullptr;
    t_currentDC = nullptr;
    // Context unbound - our state caches now point at things that no
    // longer exist. Reset them so the next bind doesn't get dedup'd.
    gdangle_invalidateAllStateCaches();
    SetLastError(ERROR_SUCCESS);
    return TRUE;
  }

  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_contexts.find(hglrc);
  if (it == g_contexts.end()) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  auto *fc = it->second;
  FakeContext *prev = t_current;
  BOOL ok = a.eglMakeCurrent(a.display, fc->surface, fc->surface, fc->eglCtx)
                ? TRUE
                : FALSE;
  if (ok) {
    // Context switched (or first bind) - drop stale state caches before
    // any further GL calls dedup against an old binding. Cocos2d's
    // CCEGLView::toggleFullScreen tears down the old context and creates
    // a new one; without this, dedup'd binds skip ANGLE calls and cause
    // null-deref crashes (e.g. glRenderbufferStorage on no-bound RBO).
    if (prev != fc) {
      gdangle_invalidateAllStateCaches();
    }
    t_current = fc;
    t_currentDC = hdc;
    // Force vsync off after MakeCurrent - ANGLE may ignore early calls
    if (Config::get().force_no_vsync && a.eglSwapInterval) {
      a.eglSwapInterval(a.display, 0);
    }
    if (!g_glInitDone) {
      g_glInitDone = true;
      gdangle_postGLInit();

      // Halfres: now that GL is up, allocate the offscreen FBO sized
      // to the backbuffer (derived from the HDC's window). No-op when
      // halfres_render disabled.
      HWND hwnd = WindowFromDC(hdc);
      if (hwnd) {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        // halfres init removed
      }
    }
    SetLastError(ERROR_SUCCESS);
  } else {
    SetLastError(ERROR_INVALID_FUNCTION);
    angle::log("eglMakeCurrent failed: 0x%x", a.eglGetError());
  }
  return ok;
}

HGLRC WINAPI wgl_wglGetCurrentContext() {
  std::lock_guard<std::mutex> lock(g_mutex);
  for (auto &kv : g_contexts) {
    if (kv.second == t_current)
      return kv.first;
  }
  return nullptr;
}

HDC WINAPI wgl_wglGetCurrentDC() { return t_currentDC; }

// Forward declarations of our WGL extension implementations
extern "C" BOOL WINAPI wgl_wglSwapIntervalEXT(int interval);
extern "C" HGLRC WINAPI wgl_wglCreateContextAttribsARB(HDC hdc,
                                                       HGLRC shareContext,
                                                       const int *attribList);
static int WINAPI wgl_wglGetSwapIntervalEXT() { return 0; }
static const char *WINAPI wgl_wglGetExtensionsStringEXT() {
  return "WGL_EXT_swap_control WGL_ARB_extensions_string "
         "WGL_EXT_extensions_string WGL_ARB_create_context "
         "WGL_ARB_create_context_profile";
}
static const char *WINAPI wgl_wglGetExtensionsStringARB(HDC) {
  return "WGL_EXT_swap_control WGL_ARB_extensions_string "
         "WGL_EXT_extensions_string WGL_ARB_create_context "
         "WGL_ARB_create_context_profile";
}

// Last-resort no-op fallback so GLEW pointer stays non-NULL for unknown gl/glu
// names. Eclipse's GLEW resolves dozens of desktop GL 3.x/4.x funcs that ANGLE
// GLES2 lacks; returning NULL leaves __glewXxx pointer null, which crashes (DEP
// at RIP=0) on call. Returns 0 in RAX so handle/bool/int returns are safely
// "null/false".
extern "C" __declspec(dllexport) intptr_t WINAPI gdangle_glNoOp() { return 0; }

PROC WINAPI wgl_wglGetProcAddress(LPCSTR name) {
  if (!name)
    return nullptr;
  auto &a = angle::state();
  static HMODULE selfMod = nullptr;
  if (!selfMod && name[0] == 'g' && name[1] == 'l') {
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       (LPCSTR)&wgl_wglGetProcAddress, &selfMod);
  }

  // PREFER our exported wrappers (so our spoofs/stubs fire).
  // This must come BEFORE ANGLE init check - Eclipse may call wglGetProcAddress
  // before our gdangle_postGLInit() completes; we should still resolve our
  // stubs.
  if (name[0] == 'g' && name[1] == 'l') {
    if (selfMod) {
      PROC p = GetProcAddress(selfMod, name);
      if (p) {
        static int sn = 0;
        if (sn < 240) {
          angle::log("wglGPA SELF %s -> %p", name, p);
          sn++;
        }
        return p;
      }
    }
  }

  // WGL extensions cocos2d / GLEW look up via wglGetProcAddress.
  // ANGLE's libGLESv2 doesn't export these - provide our own wrappers.
  if (name[0] == 'w' && name[1] == 'g' && name[2] == 'l') {
    if (!strcmp(name, "wglCreateContextAttribsARB"))
      return (PROC)wgl_wglCreateContextAttribsARB;
    if (!strcmp(name, "wglSwapIntervalEXT"))
      return (PROC)wgl_wglSwapIntervalEXT;
    if (!strcmp(name, "wglGetSwapIntervalEXT"))
      return (PROC)wgl_wglGetSwapIntervalEXT;
    if (!strcmp(name, "wglGetExtensionsStringEXT"))
      return (PROC)wgl_wglGetExtensionsStringEXT;
    if (!strcmp(name, "wglGetExtensionsStringARB"))
      return (PROC)wgl_wglGetExtensionsStringARB;
  }

  if (!a.initialized && !angle::init()) {
    angle::log("wglGetProcAddress: ANGLE not initialized for '%s'", name);
    return nullptr;
  }

  static const struct {
    const char *suffix;
    size_t len;
  } kSuffixes[] = {
      {"ARB", 3}, {"EXT", 3}, {"OES", 3}, {"ANGLE", 5}, {"NV", 2},
  };
  size_t nameLen = strlen(name);

  // Prefer our core wrappers for extension-suffixed aliases before checking
  // ANGLE's exact export table. Some ANGLE builds expose EXT/OES entry
  // points directly, but returning them would bypass our safety shims.
  if (name[0] == 'g' && name[1] == 'l' && selfMod) {
    for (auto &s : kSuffixes) {
      if (nameLen > s.len && !strcmp(name + nameLen - s.len, s.suffix)) {
        char base[128];
        size_t baseLen = nameLen - s.len;
        if (baseLen >= sizeof(base))
          break;
        memcpy(base, name, baseLen);
        base[baseLen] = '\0';
        PROC p = GetProcAddress(selfMod, base);
        if (p) {
          static int san = 0;
          if (san < 240) {
            angle::log("wglGPA SELF alias %s -> %s -> %p", name, base, p);
            san++;
          }
          return p;
        }
      }
    }
  }

  // try GLES2 DLL directly first (fast path)
  if (a.gles2) {
    PROC p = (PROC)GetProcAddress(a.gles2, name);
    if (p) {
      static int gn = 0;
      if (gn < 240) {
        angle::log("wglGPA GLES2 %s -> %p", name, p);
        gn++;
      }
      return p;
    }
  }
  // fall back to eglGetProcAddress (for GL extensions)
  if (a.eglGetProcAddress) {
    PROC p = (PROC)a.eglGetProcAddress(name);
    if (p) {
      static int en = 0;
      if (en < 240) {
        angle::log("wglGPA EGL  %s -> %p", name, p);
        en++;
      }
      return p;
    }
  }

  // If there is no local wrapper for the suffix alias, fall back to ANGLE's
  // core symbol. This keeps compatibility for harmless extension aliases.
  for (auto &s : kSuffixes) {
    if (nameLen > s.len && !strcmp(name + nameLen - s.len, s.suffix)) {
      char base[128];
      size_t baseLen = nameLen - s.len;
      if (baseLen >= sizeof(base))
        break;
      memcpy(base, name, baseLen);
      base[baseLen] = '\0';
      if (a.gles2) {
        PROC p = (PROC)GetProcAddress(a.gles2, base);
        if (p)
          return p;
      }
      if (a.eglGetProcAddress) {
        PROC p = (PROC)a.eglGetProcAddress(base);
        if (p)
          return p;
      }
    }
  }

  // CRITICAL: Eclipse + GLEW request many desktop-only GL 3.x/4.x funcs that
  // ANGLE doesn't have (glPolygonMode, glDrawElementsBaseVertex,
  // glPatchParameteri, glProgramBinary, ...). Returning NULL -> __glewXxx
  // pointer = NULL -> DEP crash. Returning a no-op is harmless: GLEW only calls
  // them if the mod actually invokes the function, and we'd rather skip a
  // render feature than crash the game.
  if (name[0] == 'g' && name[1] == 'l') {
    static int n = 0;
    if (n < 240) {
      angle::log("wglGPA STUB %s -> no-op", name);
      n++;
    }
    return (PROC)gdangle_glNoOp;
  }
  return nullptr;
}

BOOL WINAPI wgl_wglShareLists(HGLRC, HGLRC) {
  // ANGLE handles context sharing differently; GD doesn't rely on this
  return TRUE;
}

BOOL WINAPI wgl_wglSwapBuffers(HDC hdc) {
  auto &a = angle::state();
  FakeContext *fc = t_current;
  if (!fc) {
    // find by HDC
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto &kv : g_contexts) {
      if (kv.second->hdc == hdc) {
        fc = kv.second;
        break;
      }
    }
  }
  if (!fc) {
    static int logCount = 0;
    if (logCount++ < 3)
      angle::log("wglSwapBuffers: NO CONTEXT for hdc=%p", hdc);
    return FALSE;
  }

  // Frame pacing - gate this present until target_dt has elapsed.
  // Combined with allow_tearing this delivers stable target FPS
  // (e.g. 165) without DXGI vsync waits or jitter from driver scheduling.
  boost_frame_pacing::prePresent();

  // Present-skip: when present_skip_idle is enabled and no draws happened
  // since last frame, skip the actual eglSwapBuffers call.
  BOOL ok;
  if (gdangle_shouldSkipPresent()) {
    ok = TRUE; // synthesize success; driver retains previous frame
  } else {
    ok = a.eglSwapBuffers(a.display, fc->surface) ? TRUE : FALSE;
  }

  // Мégahack: invalidate frame-local GL state caches at frame boundary
  gdangle_invalidateProxyStateCaches();

  // FPS / profile sampler: per-second snapshot to fps_log.csv.
  // Microfreeze-safe: persistent FILE* (no per-second fopen/fclose hitch),
  // and per-frame max/p99 tracking so any spike is visible in the log.
  static LARGE_INTEGER s_freq = {};
  static LARGE_INTEGER s_lastWrite = {};
  static LARGE_INTEGER s_lastFrameQpc = {};
  static int s_swapsThisSec = 0;
  static unsigned long long s_lastDA = 0, s_lastDE = 0;
  static FILE *s_fpsLog = nullptr;
  static bool s_init = false;
  // Frame time histogram for this 1-second window (max 240 entries; capped
  // to avoid unbounded growth at extreme FPS). Stored as ticks for cheapness.
  static constexpr int kMaxSamples = 256;
  static LONGLONG s_dtTicks[kMaxSamples] = {};
  static int s_dtCount = 0;
  static LONGLONG s_dtMaxTicks = 0;

  if (!s_init) {
    QueryPerformanceFrequency(&s_freq);
    QueryPerformanceCounter(&s_lastWrite);
    QueryPerformanceCounter(&s_lastFrameQpc);
    s_fpsLog = std::fopen("fps_log.csv", "a");
    if (s_fpsLog) {
      std::setvbuf(s_fpsLog, nullptr, _IOFBF, 4096);
      // CSV header on first launch (only if file was empty).
      // (Cheap heuristic: just always emit; analysis tools can dedupe.)
      std::fprintf(
          s_fpsLog,
          "qpc,fps,frames,da_per_frame,de_per_frame,max_dt_ms,p99_dt_ms\n");
    }
    s_init = true;
  }
  s_swapsThisSec++;

  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);

  // Per-frame dt sample for spike tracking.
  LONGLONG dtTicks = now.QuadPart - s_lastFrameQpc.QuadPart;
  s_lastFrameQpc = now;
  if (dtTicks > s_dtMaxTicks)
    s_dtMaxTicks = dtTicks;
  if (s_dtCount < kMaxSamples)
    s_dtTicks[s_dtCount++] = dtTicks;

  LONGLONG delta = now.QuadPart - s_lastWrite.QuadPart;
  if (delta >= s_freq.QuadPart) {
    double seconds = (double)delta / (double)s_freq.QuadPart;
    double fps = (double)s_swapsThisSec / seconds;
    unsigned long long da = gdangle_getDrawArraysCount();
    unsigned long long de = gdangle_getDrawElementsCount();
    unsigned long long daPerFrame =
        s_swapsThisSec ? (da - s_lastDA) / s_swapsThisSec : 0;
    unsigned long long dePerFrame =
        s_swapsThisSec ? (de - s_lastDE) / s_swapsThisSec : 0;
    s_lastDA = da;
    s_lastDE = de;

    // Compute p99 frame time for the window (small in-place insertion sort
    // - n<=256, runs in <50 µs, off the spike path because it's only
    // executed once per second).
    double maxDtMs = (double)s_dtMaxTicks * 1000.0 / (double)s_freq.QuadPart;
    double p99DtMs = 0.0;
    if (s_dtCount > 0) {
      // Insertion sort ascending.
      for (int i = 1; i < s_dtCount; i++) {
        LONGLONG key = s_dtTicks[i];
        int j = i - 1;
        while (j >= 0 && s_dtTicks[j] > key) {
          s_dtTicks[j + 1] = s_dtTicks[j];
          --j;
        }
        s_dtTicks[j + 1] = key;
      }
      int p99idx = (s_dtCount * 99) / 100;
      if (p99idx >= s_dtCount)
        p99idx = s_dtCount - 1;
      p99DtMs = (double)s_dtTicks[p99idx] * 1000.0 / (double)s_freq.QuadPart;
    }

    if (s_fpsLog) {
      std::fprintf(s_fpsLog, "%lld,%.2f,%d,%llu,%llu,%.2f,%.2f\n",
                   (long long)now.QuadPart, fps, s_swapsThisSec, daPerFrame,
                   dePerFrame, maxDtMs, p99DtMs);
      // fflush every 5 s - pushes the CRT buffer to the OS write cache.
      // This is a pure WriteFile (no metadata journal, no AV scan), so it
      // costs <0.1 ms - orders of magnitude cheaper than the original
      // fopen/fclose-per-second pattern that caused the microfreeze.
      // Ensures data survives a hard kill (e.g. Task Manager / Kill()).
      static int s_flushCounter = 0;
      if (++s_flushCounter >= 5) {
        std::fflush(s_fpsLog);
        s_flushCounter = 0;
      }
    }
    s_swapsThisSec = 0;
    s_lastWrite = now;
    s_dtCount = 0;
    s_dtMaxTicks = 0;
  }
  return ok;
}

BOOL WINAPI wgl_wglSwapIntervalEXT(int interval) {
  auto &a = angle::state();
  if (!a.eglSwapInterval)
    return FALSE;
  // If force_no_vsync is enabled, ignore caller's request and force 0 - defends
  // against cocos2d / mods that re-enable vsync after init.
  if (Config::get().force_no_vsync && interval != 0) {
    static bool s_loggedOverride = false;
    if (!s_loggedOverride) {
      angle::log("wglSwapIntervalEXT: caller requested %d, forced to 0 "
                 "(force_no_vsync)",
                 interval);
      s_loggedOverride = true;
    }
    interval = 0;
  }
  return a.eglSwapInterval(a.display, interval) ? TRUE : FALSE;
}

HGLRC WINAPI wgl_wglCreateContextAttribsARB(HDC hdc, HGLRC shareContext,
                                            const int *attribList) {
  (void)shareContext;
  (void)attribList;
  angle::log("wglCreateContextAttribsARB: forwarding to wglCreateContext");
  return wgl_wglCreateContext(hdc);
}

// pixel format - we don't really negotiate, we just accept whatever GD asks
int WINAPI wgl_wglChoosePixelFormat(HDC, const PIXELFORMATDESCRIPTOR *) {
  return 1;
}

BOOL WINAPI wgl_wglSetPixelFormat(HDC, int, const PIXELFORMATDESCRIPTOR *) {
  return TRUE;
}

int WINAPI wgl_wglDescribePixelFormat(HDC, int, UINT size,
                                      LPPIXELFORMATDESCRIPTOR ppfd) {
  if (ppfd && size >= sizeof(PIXELFORMATDESCRIPTOR)) {
    ZeroMemory(ppfd, sizeof(*ppfd));
    ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
    ppfd->nVersion = 1;
    ppfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    ppfd->iPixelType = PFD_TYPE_RGBA;
    ppfd->cColorBits = 32;
    ppfd->cDepthBits = 24;
    ppfd->cStencilBits = 8;
  }
  return 1;
}

int WINAPI wgl_wglGetPixelFormat(HDC) { return 1; }

} // extern "C"
