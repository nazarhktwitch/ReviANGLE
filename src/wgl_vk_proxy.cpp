// Vulkan WGL proxy - implements WGL entry points using Vulkan backend
// Analogous to wgl_proxy.cpp but routes to Vulkan instead of ANGLE/DirectX

#include <windows.h>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <cstring>
#include "vk_proxy.hpp"
#include "angle_loader.hpp"
#include "config.hpp"

// External callback (matches DirectX version)
extern void gdangle_postGLInit();

// Global context tracking
static std::unordered_map<HGLRC, std::unique_ptr<vkproxy::VulkanContext>> g_vk_contexts;
static std::mutex g_vk_mutex;

static thread_local vkproxy::VulkanContext* t_vk_current = nullptr;
static thread_local HDC t_vk_currentDC = nullptr;

extern "C" BOOL WINAPI wgl_wglSwapIntervalEXT(int interval);
extern "C" __declspec(dllexport) intptr_t WINAPI gdangle_glNoOp();

static int WINAPI wgl_wglGetSwapIntervalEXT() {
    return 0;
}

static const char* WINAPI wgl_wglGetExtensionsStringEXT() {
    return "WGL_EXT_swap_control WGL_ARB_extensions_string WGL_EXT_extensions_string";
}

static const char* WINAPI wgl_wglGetExtensionsStringARB(HDC) {
    return "WGL_EXT_swap_control WGL_ARB_extensions_string WGL_EXT_extensions_string";
}

extern "C" {

HGLRC WINAPI wgl_wglCreateContextAttribsARB(HDC hdc, HGLRC shareContext, const int* attribList);

HGLRC WINAPI wgl_wglCreateContext(HDC hdc) {
    vkproxy::VulkanState& vk = vkproxy::VulkanState::getInstance();

    HWND hwnd = WindowFromDC(hdc);
    if (hwnd) {
        char cls[64] = {}, ttl[128] = {};
        GetClassNameA(hwnd, cls, 63);
        GetWindowTextA(hwnd, ttl, 127);
        RECT r = {}; GetWindowRect(hwnd, &r);
        HWND parent = GetParent(hwnd);
        BOOL vis = IsWindowVisible(hwnd);
        DWORD style = (DWORD)GetWindowLongPtrA(hwnd, GWL_STYLE);
        angle::log("wgl_wglCreateContext: hdc=%p hwnd=%p cls='%s' title='%s' parent=%p vis=%d style=0x%X size=%dx%d",
                   hdc, hwnd, cls, ttl, parent, vis, style, r.right-r.left, r.bottom-r.top);
    } else {
        angle::log("wgl_wglCreateContext: dummy HDC %p (no HWND)", hdc);
    }

    auto ctx = vk.createContext(hdc, hwnd);
    if (!ctx) {
        angle::log("wgl_wglCreateContext: creating dummy context fallback");
        ctx = std::make_unique<vkproxy::VulkanContext>();
        ctx->hdc = hdc;
        ctx->hwnd = hwnd;
    }

    std::lock_guard<std::mutex> lock(g_vk_mutex);
    HGLRC fake = (HGLRC)(uintptr_t)(g_vk_contexts.size() + 1);
    while (g_vk_contexts.count(fake)) {
        fake = (HGLRC)((uintptr_t)fake + 1);
    }

    auto* rawCtx = ctx.get();
    g_vk_contexts[fake] = std::move(ctx);

    angle::log("wgl_wglCreateContext -> %p (vk_surface=%p)", fake, rawCtx->surface);
    return fake;
}

HGLRC WINAPI wgl_wglCreateContextAttribsARB(HDC hdc, HGLRC shareContext, const int* attribList) {
    angle::log("wglCreateContextAttribsARB called for HDC %p", hdc);
    return wgl_wglCreateContext(hdc);
}

BOOL WINAPI wgl_wglDeleteContext(HGLRC hglrc) {
    std::lock_guard<std::mutex> lock(g_vk_mutex);
    auto it = g_vk_contexts.find(hglrc);
    if (it == g_vk_contexts.end()) {
        return FALSE;
    }

    auto& ctx = it->second;
    vkproxy::VulkanState& vk = vkproxy::VulkanState::getInstance();
    VkDevice device = vk.getDevice();

    // Cleanup Vulkan resources
    if (ctx->inFlightFence != VK_NULL_HANDLE) {
        vkDestroyFence(device, ctx->inFlightFence, nullptr);
    }
    if (ctx->renderFinishedSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, ctx->renderFinishedSemaphore, nullptr);
    }
    if (ctx->imageAvailableSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, ctx->imageAvailableSemaphore, nullptr);
    }
    for (auto& framebuffer : ctx->framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
    }
    for (auto& imageView : ctx->swapchainImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, imageView, nullptr);
        }
    }
    if (ctx->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, ctx->swapchain, nullptr);
    }
    if (ctx->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(vk.getVkInstance(), ctx->surface, nullptr);
    }
    if (ctx->cmdPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, ctx->cmdPool, nullptr);
    }

    vkproxy::VulkanContext* deletedCtx = ctx.get();
    g_vk_contexts.erase(it);

    if (t_vk_current == deletedCtx) {
        t_vk_current = nullptr;
        t_vk_currentDC = nullptr;
    }

    angle::log("wgl_wglDeleteContext: %p deleted", hglrc);
    return TRUE;
}

BOOL WINAPI wgl_wglMakeCurrent(HDC hdc, HGLRC hglrc) {
    if (!hglrc) {
        t_vk_current = nullptr;
        t_vk_currentDC = nullptr;
        return TRUE;
    }

    std::lock_guard<std::mutex> lock(g_vk_mutex);
    auto it = g_vk_contexts.find(hglrc);
    if (it == g_vk_contexts.end()) {
        return FALSE;
    }

    vkproxy::VulkanState& vk = vkproxy::VulkanState::getInstance();
    vk.makeContextCurrent(it->second.get());

    t_vk_current = it->second.get();
    t_vk_currentDC = hdc;

    angle::log("wgl_wglMakeCurrent: hdc=%p hglrc=%p", hdc, hglrc);

    // Call post-GL initialization (once, on first MakeCurrent)
    static bool g_initialized = false;
    if (!g_initialized) {
        g_initialized = true;
        angle::log("GPU active: Vulkan backend");
        gdangle_postGLInit();
    }

    return TRUE;
}

HGLRC WINAPI wgl_wglGetCurrentContext() {
    // Return a fake handle if context is current
    return t_vk_current ? (HGLRC)0x1 : nullptr;
}

HDC WINAPI wgl_wglGetCurrentDC() {
    return t_vk_currentDC;
}

PROC WINAPI wgl_wglGetProcAddress(LPCSTR name) {
    if (!name) {
        return nullptr;
    }

    if (name[0] == 'w' && name[1] == 'g' && name[2] == 'l') {
        if (!std::strcmp(name, "wglCreateContextAttribsARB")) {
            return reinterpret_cast<PROC>(wgl_wglCreateContextAttribsARB);
        }
        if (!std::strcmp(name, "wglSwapIntervalEXT")) {
            return reinterpret_cast<PROC>(wgl_wglSwapIntervalEXT);
        }
        if (!std::strcmp(name, "wglGetSwapIntervalEXT")) {
            return reinterpret_cast<PROC>(wgl_wglGetSwapIntervalEXT);
        }
        if (!std::strcmp(name, "wglGetExtensionsStringEXT")) {
            return reinterpret_cast<PROC>(wgl_wglGetExtensionsStringEXT);
        }
        if (!std::strcmp(name, "wglGetExtensionsStringARB")) {
            return reinterpret_cast<PROC>(wgl_wglGetExtensionsStringARB);
        }
    }

    // Prefer our exported GL wrappers so cocos2d/GLEW never cache a null
    // function pointer and call through address 0.
    if (name[0] == 'g' && name[1] == 'l') {
        static HMODULE selfMod = nullptr;
        if (!selfMod) {
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                               reinterpret_cast<LPCSTR>(&wgl_wglGetProcAddress),
                               &selfMod);
        }
        if (selfMod) {
            PROC p = GetProcAddress(selfMod, name);
            if (p) {
                return p;
            }
        }

        return reinterpret_cast<PROC>(gdangle_glNoOp);
    }

    return nullptr;
}

__declspec(dllexport) intptr_t WINAPI gdangle_glNoOp() {
    return 0;
}

BOOL WINAPI wgl_wglShareLists(HGLRC a, HGLRC b) {
    // Vulkan contexts can share resources if needed
    return TRUE;
}

namespace boost_stutter_monitor {
void onFrame();
}

BOOL WINAPI wgl_wglSwapBuffers(HDC hdc) {
    if (!t_vk_current) {
        return FALSE;
    }

    boost_stutter_monitor::onFrame();
    vkproxy::VulkanState& vk = vkproxy::VulkanState::getInstance();
    return vk.presentFrame(t_vk_current) ? TRUE : FALSE;
}

BOOL WINAPI wgl_wglSwapIntervalEXT(int interval) {
    // Vulkan presentation mode handling
    // In a full implementation, this would control vsync
    angle::log("wgl_wglSwapIntervalEXT: %d (Vulkan: not implemented)", interval);
    return TRUE;
}

int WINAPI wgl_wglChoosePixelFormat(HDC hdc, const PIXELFORMATDESCRIPTOR* ppfd) {
    // Return a dummy pixel format ID
    return 1;
}

BOOL WINAPI wgl_wglSetPixelFormat(HDC hdc, int format, const PIXELFORMATDESCRIPTOR* ppfd) {
    return TRUE;
}

int WINAPI wgl_wglDescribePixelFormat(HDC hdc, int format, UINT size, LPPIXELFORMATDESCRIPTOR ppfd) {
    if (!ppfd || size < sizeof(PIXELFORMATDESCRIPTOR)) {
        return 1; // Return number of formats
    }

    ZeroMemory(ppfd, size);
    ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
    ppfd->nVersion = 1;
    ppfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    ppfd->iPixelType = PFD_TYPE_RGBA;
    ppfd->cColorBits = 32;
    ppfd->cDepthBits = 24;
    ppfd->cStencilBits = 8;
    ppfd->iLayerType = PFD_MAIN_PLANE;

    return 1;
}

int WINAPI wgl_wglGetPixelFormat(HDC hdc) {
    return 1;
}

} // extern "C"
