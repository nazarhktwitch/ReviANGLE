// reviangle-studio.exe - entry point.
//
// ReviANGLE Studio  -  Config Editor for the ReviANGLE Performance Suite
// Win32 + DX11 + Dear ImGui. Boilerplate is adapted from ImGui's
// example_win32_directx11/main.cpp; logic-specific bits are minimal because
// EditorApp owns all of the UI.

#include "editor_app.hpp"

#include <d3d11.h>
#include <filesystem>
#include <string>
#include <tchar.h>
#include <windows.h>

#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "imgui.h"

// --------------- DX11 globals ------------------------------------------------
static ID3D11Device *g_d3dDevice = nullptr;
static ID3D11DeviceContext *g_d3dContext = nullptr;
static IDXGISwapChain *g_swapChain = nullptr;
static ID3D11RenderTargetView *g_rtv = nullptr;
static UINT g_resizeW = 0;
static UINT g_resizeH = 0;

static bool createDevice(HWND hWnd);
static void cleanupDevice();
static void createRTV();
static void cleanupRTV();

// ImGui Win32 backend forward (defined inside imgui_impl_win32.cpp).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM,
                                                             LPARAM);

static EditorApp *g_app = nullptr;

static LRESULT WINAPI wndProc(HWND hWnd, UINT msg, WPARAM wParam,
                              LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;

  switch (msg) {
  case WM_SIZE:
    if (wParam == SIZE_MINIMIZED)
      return 0;
    g_resizeW = (UINT)LOWORD(lParam);
    g_resizeH = (UINT)HIWORD(lParam);
    return 0;
  case WM_SYSCOMMAND:
    // Disable Alt-application menu.
    if ((wParam & 0xfff0) == SC_KEYMENU)
      return 0;
    break;
  case WM_DESTROY:
    ::PostQuitMessage(0);
    return 0;
  case WM_CLOSE:
    if (g_app)
      g_app->requestClose();
    ::PostQuitMessage(0);
    return 0;
  }
  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// --------------- DX11 helpers ------------------------------------------------
static bool createDevice(HWND hWnd) {
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT createFlags = 0;
  D3D_FEATURE_LEVEL featureLevel;
  const D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_0,
  };
  HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags, levels,
      _countof(levels), D3D11_SDK_VERSION, &sd, &g_swapChain, &g_d3dDevice,
      &featureLevel, &g_d3dContext);
  if (hr == DXGI_ERROR_UNSUPPORTED) {
    // Fall back to WARP (software) if no GPU available.
    hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createFlags, levels,
        _countof(levels), D3D11_SDK_VERSION, &sd, &g_swapChain, &g_d3dDevice,
        &featureLevel, &g_d3dContext);
  }
  if (FAILED(hr))
    return false;

  createRTV();
  return true;
}

static void cleanupDevice() {
  cleanupRTV();
  if (g_swapChain) {
    g_swapChain->Release();
    g_swapChain = nullptr;
  }
  if (g_d3dContext) {
    g_d3dContext->Release();
    g_d3dContext = nullptr;
  }
  if (g_d3dDevice) {
    g_d3dDevice->Release();
    g_d3dDevice = nullptr;
  }
}

static void createRTV() {
  ID3D11Texture2D *backBuffer = nullptr;
  g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
  if (backBuffer) {
    g_d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);
    backBuffer->Release();
  }
}
static void cleanupRTV() {
  if (g_rtv) {
    g_rtv->Release();
    g_rtv = nullptr;
  }
}

// --------------- INI path ----------------------------------------------------
// Strategy: try angle_config.ini in the current working dir first; if it
// doesn't exist, try alongside the editor exe itself; finally fall back to
// the cwd path so the editor still saves there.
static std::string resolveIniPath() {
  namespace fs = std::filesystem;
  if (fs::exists("angle_config.ini"))
    return "angle_config.ini";

  wchar_t exeW[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exeW, MAX_PATH);
  fs::path exePath(exeW);
  fs::path beside = exePath.parent_path() / L"angle_config.ini";
  if (fs::exists(beside))
    return beside.string();

  return "angle_config.ini";
}

// Apply a custom dark theme tuned for the editor.
static void applyTheme() {
  ImGuiStyle &s = ImGui::GetStyle();
  ImGui::StyleColorsDark();
  s.WindowRounding = 6.0f;
  s.FrameRounding = 4.0f;
  s.GrabRounding = 3.0f;
  s.ScrollbarRounding = 4.0f;
  s.TabRounding = 4.0f;
  s.ItemSpacing = ImVec2(8, 6);
  s.FramePadding = ImVec2(8, 4);

  auto &c = s.Colors;
  c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.0f);
  c[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
  c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);
  c[ImGuiCol_Border] = ImVec4(0.20f, 0.22f, 0.26f, 1.0f);
  c[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.20f, 1.0f);
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.24f, 0.28f, 1.0f);
  c[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.30f, 0.36f, 1.0f);
  c[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
  c[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
  c[ImGuiCol_Tab] = ImVec4(0.14f, 0.16f, 0.20f, 1.0f);
  c[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.40f, 0.55f, 1.0f);
  // TabSelected, TabDimmed, TabDimmedSelected not available in this ImGui
  // version
  c[ImGuiCol_Header] = ImVec4(0.20f, 0.30f, 0.45f, 0.45f);
  c[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.35f, 0.55f, 0.65f);
  c[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.40f, 0.60f, 1.0f);
  c[ImGuiCol_Button] = ImVec4(0.18f, 0.20f, 0.25f, 1.0f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.34f, 0.42f, 1.0f);
  c[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.44f, 0.52f, 1.0f);
  c[ImGuiCol_CheckMark] = ImVec4(0.34f, 0.85f, 0.45f, 1.0f);
  c[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.55f, 0.85f, 1.0f);
  c[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.65f, 0.95f, 1.0f);
  c[ImGuiCol_Separator] = ImVec4(0.20f, 0.22f, 0.26f, 1.0f);
}

// --------------- Entry point -------------------------------------------------
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_CLASSDC;
  wc.lpfnWndProc = wndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"GdAngleEditor";
  ::RegisterClassExW(&wc);

  HWND hWnd = ::CreateWindowW(wc.lpszClassName, L"ReviANGLE Studio",
                              WS_OVERLAPPEDWINDOW, 100, 100, 1200, 760, nullptr,
                              nullptr, hInstance, nullptr);

  if (!createDevice(hWnd)) {
    ::DestroyWindow(hWnd);
    ::UnregisterClassW(wc.lpszClassName, hInstance);
    ::MessageBoxW(nullptr, L"Failed to initialise Direct3D 11.",
                  L"ReviANGLE Studio", MB_OK | MB_ICONERROR);
    return 1;
  }

  ::ShowWindow(hWnd, SW_SHOWDEFAULT);
  ::UpdateWindow(hWnd);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr; // don't write imgui.ini next to the exe

  // Cyrillic support: ImGui's default font (ProggyClean) is Latin-only, so
  // Russian descriptions render as `?` boxes. Load Segoe UI from the system
  // fonts folder with the Cyrillic glyph range merged in.
  //
  // IMPORTANT: only request glyph ranges we actually need. Building the
  // atlas with 20k+ glyphs (e.g. the entire U+2010..U+25FF range) overflows
  // the default 1024x1024 texture and ImGui silently falls back to a broken
  // state, crashing on first frame.
  {
    wchar_t winDirW[MAX_PATH] = {};
    GetWindowsDirectoryW(winDirW, MAX_PATH);
    char winDir[MAX_PATH] = {};
    WideCharToMultiByte(CP_UTF8, 0, winDirW, -1, winDir, MAX_PATH, nullptr,
                        nullptr);

    char fontPath[MAX_PATH] = {};
    std::snprintf(fontPath, MAX_PATH, "%s\\Fonts\\segoeui.ttf", winDir);

    // Static range table: Default (Latin) + Cyrillic + a tiny set of
    // bullet/checkmark/dot glyphs used in our UI strings.
    // Each pair is {first, last} inclusive; terminated with {0, 0}.
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
        0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
        0x2010, 0x205E, // General Punctuation (en-dash, em-dash, …)
        0x2122, 0x2122, // ™
        0x2190, 0x21FF, // Arrows (← → ↑ ↓ ↔ ▸ etc.)
        0x2500, 0x257F, // Box Drawing (│ ┌ ┐ ─ etc.)
        0x25A0, 0x25FF, // Geometric Shapes (● ◆ ▪ ◯ etc.)
        0x2713, 0x2717, // ✓ ✗
        0,
    };

    ImFontConfig cfg{};
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;
    // 17 px is readable for both English and Russian on 1080p.
    ImFont *loaded =
        io.Fonts->AddFontFromFileTTF(fontPath, 17.0f, &cfg, ranges);
    if (!loaded) {
      // Fallback: keep ImGui's built-in font (Latin-only) so the editor
      // at least launches if Segoe UI isn't present.
      io.Fonts->AddFontDefault();
    }
  }

  applyTheme();

  ImGui_ImplWin32_Init(hWnd);
  ImGui_ImplDX11_Init(g_d3dDevice, g_d3dContext);

  EditorApp app;
  g_app = &app;
  if (!app.init(resolveIniPath())) {
    ::MessageBoxW(hWnd, L"Failed to load angle_config.ini.",
                  L"ReviANGLE Studio", MB_OK | MB_ICONWARNING);
  }

  bool done = false;
  while (!done && app.isRunning()) {
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
      if (msg.message == WM_QUIT)
        done = true;
    }
    if (done)
      break;

    if (g_resizeW != 0 && g_resizeH != 0) {
      cleanupRTV();
      g_swapChain->ResizeBuffers(0, g_resizeW, g_resizeH, DXGI_FORMAT_UNKNOWN,
                                 0);
      g_resizeW = g_resizeH = 0;
      createRTV();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    app.renderFrame();

    ImGui::Render();
    const float clearColor[4] = {0.05f, 0.05f, 0.07f, 1.0f};
    g_d3dContext->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_d3dContext->ClearRenderTargetView(g_rtv, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swapChain->Present(1, 0); // vsync - editor doesn't need >60 FPS
  }

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  cleanupDevice();
  ::DestroyWindow(hWnd);
  ::UnregisterClassW(wc.lpszClassName, hInstance);
  return 0;
}
