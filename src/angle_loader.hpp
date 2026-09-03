#pragma once
#include <windows.h>

// opaque ANGLE handles (we don't ship the real EGL headers here)
typedef void*  EGLDisplay_t;
typedef void*  EGLContext_t;
typedef void*  EGLSurface_t;
typedef void*  EGLConfig_t;
typedef int    EGLBoolean_t;
typedef int    EGLint_t;

namespace angle {

    struct Loaded {
        HMODULE egl   = nullptr;
        HMODULE gles2 = nullptr;

        // function pointers we cache
        EGLDisplay_t (*eglGetPlatformDisplayEXT)(EGLint_t, void*, const EGLint_t*) = nullptr;
        EGLDisplay_t (*eglGetDisplay)(void*) = nullptr;
        EGLBoolean_t (*eglInitialize)(EGLDisplay_t, EGLint_t*, EGLint_t*) = nullptr;
        EGLBoolean_t (*eglChooseConfig)(EGLDisplay_t, const EGLint_t*, EGLConfig_t*, EGLint_t, EGLint_t*) = nullptr;
        EGLSurface_t (*eglCreateWindowSurface)(EGLDisplay_t, EGLConfig_t, HWND, const EGLint_t*) = nullptr;
        EGLSurface_t (*eglCreatePbufferSurface)(EGLDisplay_t, EGLConfig_t, const EGLint_t*) = nullptr;
        EGLContext_t (*eglCreateContext)(EGLDisplay_t, EGLConfig_t, EGLContext_t, const EGLint_t*) = nullptr;
        EGLBoolean_t (*eglMakeCurrent)(EGLDisplay_t, EGLSurface_t, EGLSurface_t, EGLContext_t) = nullptr;
        EGLBoolean_t (*eglSwapBuffers)(EGLDisplay_t, EGLSurface_t) = nullptr;
        EGLBoolean_t (*eglSwapInterval)(EGLDisplay_t, EGLint_t) = nullptr;
        EGLBoolean_t (*eglDestroyContext)(EGLDisplay_t, EGLContext_t) = nullptr;
        EGLBoolean_t (*eglDestroySurface)(EGLDisplay_t, EGLSurface_t) = nullptr;
        EGLBoolean_t (*eglTerminate)(EGLDisplay_t) = nullptr;
        void*        (*eglGetProcAddress)(const char*) = nullptr;
        EGLint_t     (*eglGetError)() = nullptr;

        EGLDisplay_t display = nullptr;
        bool         initialized = false;
    };

    bool    init();              // loads DLLs, prepares display with selected backend
    void    shutdown();
    Loaded& state();
    void    log(const char* fmt, ...);
    void    forceLog(const char* fmt, ...);   // always writes, ignores debug flag
    void    setModuleHandle(HMODULE h);

} // namespace angle
