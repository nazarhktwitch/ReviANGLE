#include "boost_overlay.hpp"
#include "angle_loader.hpp"
#include "config.hpp"
#include "ini_parser.hpp"

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>

#include <atomic>
#include <chrono>
#include <string>
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

namespace boost_overlay {

static std::atomic<bool> s_initialized{false};
static std::atomic<bool> s_visible{false};
static HWND s_hwnd = nullptr;
static WNDPROC s_origWndProc = nullptr;

static std::chrono::steady_clock::time_point s_lastToggleTime;
static std::string s_statusMessage = "";
static std::chrono::steady_clock::time_point s_statusTime;

static float s_frameTimes[120] = {};
static int s_frameTimeIdx = 0;
static float s_peakFrameTime = 0.0f;
static int s_stutterCount = 0;
static auto s_lastFrameClock = std::chrono::high_resolution_clock::now();

// Win32 WndProc hook to capture mouse & keyboard input when overlay is visible
static LRESULT CALLBACK Hooked_WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam) {
  if (s_visible.load(std::memory_order_relaxed)) {
    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
      // Suppress WM_KEYDOWN / WM_KEYUP / mouse clicks from GD while overlay is
      // active
      switch (msg) {
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_RBUTTONDOWN:
      case WM_RBUTTONUP:
      case WM_MBUTTONDOWN:
      case WM_MBUTTONUP:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_CHAR:
        return 0;
      }
    }
  }
  return CallWindowProcA(s_origWndProc, hwnd, msg, wParam, lParam);
}

static void applyDarkGlassTheme() {
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.ChildRounding = 6.0f;
  style.FrameRounding = 5.0f;
  style.PopupRounding = 6.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding = 4.0f;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.ItemSpacing = ImVec2(10.0f, 8.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);

  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_Text] = ImVec4(0.92f, 0.93f, 0.96f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.58f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.10f, 0.94f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.12f, 0.15f, 0.75f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.12f, 0.95f);
  colors[ImGuiCol_Border] = ImVec4(0.22f, 0.24f, 0.30f, 0.60f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.15f, 0.19f, 0.85f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.22f, 0.28f, 0.90f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.28f, 0.35f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.13f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.07f, 0.08f, 0.10f, 0.75f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.11f, 0.12f, 0.15f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.09f, 0.11f, 0.60f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.28f, 0.35f, 0.80f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.32f, 0.36f, 0.45f, 0.90f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.38f, 0.43f, 0.55f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.22f, 0.54f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.22f, 0.54f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.35f, 0.64f, 1.00f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.22f, 0.46f, 0.90f, 0.75f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.54f, 1.00f, 0.90f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.60f, 1.00f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.22f, 0.46f, 0.90f, 0.45f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.54f, 1.00f, 0.65f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.60f, 1.00f, 0.85f);
  colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.24f, 0.30f, 0.60f);
  colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.15f, 0.19f, 0.85f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.54f, 1.00f, 0.80f);
  colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.46f, 0.90f, 0.90f);
}

static void saveCurrentSettingsToDisk() {
  Config &cfg = Config::get();
  Ini ini;
  std::string path = "angle_config.ini";
  if (!ini.load(path)) {
    s_statusMessage = "Error: angle_config.ini not found!";
    s_statusTime = std::chrono::steady_clock::now();
    return;
  }

  // Save live parameters
  ini.set("HUD", "show_fps", cfg.show_fps ? "true" : "false");
  ini.set("HUD", "show_drawcalls", cfg.show_drawcalls ? "true" : "false");
  ini.set("HUD", "show_frame_time", cfg.show_frame_time ? "true" : "false");

  ini.set("BoostCocos", "particle_throttle",
          cfg.particle_throttle ? "true" : "false");
  ini.set("BoostCocos", "particle_max", std::to_string(cfg.particle_max));
  ini.set("BoostGD", "skip_shake_flash",
          cfg.skip_shake_flash ? "true" : "false");

  ini.set("BoostLatency", "frame_pacing", cfg.frame_pacing ? "true" : "false");
  ini.set("BoostLatency", "frame_pacing_target",
          std::to_string(cfg.frame_pacing_target));

  ini.set("BoostAdvanced", "gl_state_dedup",
          cfg.gl_state_dedup ? "true" : "false");
  ini.set("BoostRender", "mipmap_off", cfg.mipmap_off ? "true" : "false");
  ini.set("BoostRenderAdv", "disable_aa", cfg.disable_aa ? "true" : "false");

  ini.set("BoostStutterMonitor", "stutter_monitor",
          cfg.stutter_monitor ? "true" : "false");

  if (ini.save(path)) {
    s_statusMessage = "Successfully saved to angle_config.ini!";
  } else {
    s_statusMessage = "Failed to write angle_config.ini!";
  }
  s_statusTime = std::chrono::steady_clock::now();
}

static void checkHotkeyToggle() {
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                            s_lastToggleTime)
          .count() < 250) {
    return;
  }

  // Default hotkey: Alt + Home
  bool isAltPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
  bool isHomePressed = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;

  if (isAltPressed && isHomePressed) {
    s_visible.store(!s_visible.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
    s_lastToggleTime = now;
    angle::log("boost_overlay: toggled -> %s",
               s_visible.load() ? "VISIBLE" : "HIDDEN");
  }
}

void apply() {
  // Initialization done lazily on first render
}

void render(HDC hdc) {
  checkHotkeyToggle();

  if (!s_initialized.load(std::memory_order_relaxed)) {
    s_hwnd = WindowFromDC(hdc);
    if (!s_hwnd) {
      s_hwnd = GetActiveWindow();
    }
    if (!s_hwnd)
      return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // Disable imgui.ini creation in GD folder

    applyDarkGlassTheme();

    ImGui_ImplWin32_Init(s_hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Install WndProc hook
    s_origWndProc = (WNDPROC)SetWindowLongPtrA(s_hwnd, GWLP_WNDPROC,
                                               (LONG_PTR)Hooked_WndProc);

    s_initialized.store(true, std::memory_order_release);
    angle::log("boost_overlay: ImGui overlay initialized successfully");
  }

  if (!s_visible.load(std::memory_order_relaxed)) {
    return;
  }

  Config &cfg = Config::get();

  // Calculate frame time for profiler
  auto frameNow = std::chrono::high_resolution_clock::now();
  float dtMs =
      std::chrono::duration<float, std::milli>(frameNow - s_lastFrameClock)
          .count();
  s_lastFrameClock = frameNow;

  if (dtMs > 0.01f && dtMs < 1000.0f) {
    s_frameTimes[s_frameTimeIdx] = dtMs;
    s_frameTimeIdx = (s_frameTimeIdx + 1) % 120;
    if (dtMs > s_peakFrameTime) {
      s_peakFrameTime = dtMs;
    }
    float targetThresholdMs =
        (cfg.frame_pacing && cfg.frame_pacing_target > 0)
            ? (1000.0f / (float)cfg.frame_pacing_target * 1.4f)
            : 20.0f;
    if (dtMs > targetThresholdMs) {
      s_stutterCount++;
    }
  }

  // Start Frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(ImVec2(540, 520), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);

  if (ImGui::Begin("ReviANGLE Studio (In-Game Configurator)", nullptr,
                   ImGuiWindowFlags_NoCollapse)) {

    ImGui::TextColored(ImVec4(0.35f, 0.64f, 1.00f, 1.00f), "Live Tuning!");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("HUD & Statistics",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Show FPS Counter", &cfg.show_fps);
      ImGui::Checkbox("Show Draw Calls Counter", &cfg.show_drawcalls);
      ImGui::Checkbox("Show Frame Time Graph", &cfg.show_frame_time);
    }

    if (ImGui::CollapsingHeader("Cocos2d & Particles",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Throttle Max Particles", &cfg.particle_throttle);
      if (cfg.particle_throttle) {
        ImGui::SliderInt("Particle Ceiling", &cfg.particle_max, 10, 1000);
      }
      ImGui::Checkbox("Skip Screen Flash & Camera Shake",
                      &cfg.skip_shake_flash);
    }

    if (ImGui::CollapsingHeader("Latency & Frame Pacing",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("QPC High-Precision Frame Pacer", &cfg.frame_pacing);
      if (cfg.frame_pacing) {
        ImGui::SliderInt("Target FPS Cap (0 = Uncapped)",
                         &cfg.frame_pacing_target, 0, 360);
      }
    }

    if (ImGui::CollapsingHeader("Render Optimizations",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("GL State Deduplication", &cfg.gl_state_dedup);
      ImGui::Checkbox("Skip Mipmap Generation", &cfg.mipmap_off);
      ImGui::Checkbox("Disable MSAA / Anti-Aliasing", &cfg.disable_aa);
    }

    if (ImGui::CollapsingHeader("Stutter Monitor & Mini-Profiler",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Enable Stutter Monitor Logging", &cfg.stutter_monitor);
      ImGui::PlotHistogram("Frame Time (ms)", s_frameTimes, 120, s_frameTimeIdx,
                           nullptr, 0.0f, 33.3f, ImVec2(0, 70));
      ImGui::Text("Peak Frame Time: %.2f ms", s_peakFrameTime);
      ImGui::SameLine();
      if (s_stutterCount > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "  Stutters: %d",
                           s_stutterCount);
      } else {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                           "  Stutters: 0 (Smooth)");
      }
      if (ImGui::Button("Reset Profiler Stats")) {
        s_peakFrameTime = 0.0f;
        s_stutterCount = 0;
      }
    }

    if (ImGui::CollapsingHeader("RAM & Memory Optimizer")) {
      ImGui::Text("Free unneeded process memory and trim system working set.");
      if (ImGui::Button("Trim RAM Working Set", ImVec2(220, 28))) {
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
        HeapCompact(GetProcessHeap(), 0);
        s_statusMessage = "RAM Working Set successfully trimmed!";
        s_statusTime = std::chrono::steady_clock::now();
      }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Save", ImVec2(240, 32))) {
      saveCurrentSettingsToDisk();
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(160, 32))) {
      s_visible.store(false, std::memory_order_relaxed);
    }

    auto now = std::chrono::steady_clock::now();
    if (!s_statusMessage.empty() &&
        std::chrono::duration_cast<std::chrono::seconds>(now - s_statusTime)
                .count() < 4) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.40f, 1.00f), "%s",
                         s_statusMessage.c_str());
    }
  }
  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool isVisible() { return s_visible.load(std::memory_order_relaxed); }

void shutdown() {
  if (s_initialized.load(std::memory_order_relaxed)) {
    if (s_hwnd && s_origWndProc) {
      SetWindowLongPtrA(s_hwnd, GWLP_WNDPROC, (LONG_PTR)s_origWndProc);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    s_initialized.store(false, std::memory_order_relaxed);
  }
}

} // namespace boost_overlay
