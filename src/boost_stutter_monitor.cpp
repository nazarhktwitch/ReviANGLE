#include "config.hpp"
#include "angle_loader.hpp"
#include <windows.h>
#include <cstdio>
#include <chrono>

namespace boost_stutter_monitor {
    static FILE *s_logFile = nullptr;
    static unsigned long s_frameCount = 0;
    static auto s_lastFrameTime = std::chrono::high_resolution_clock::now();

    void apply() {
        if (!Config::get().stutter_monitor) return;
        angle::log("stutter_monitor: logging enabled -> writing stutters to angle_stutters.log");
        s_logFile = std::fopen("angle_stutters.log", "w");
        if (s_logFile) {
            std::fprintf(s_logFile, "=== ReviANGLE Stutter Monitor Log ===\n");
            std::fflush(s_logFile);
        }
    }

    void onFrame() {
        if (!Config::get().stutter_monitor) return;

        s_frameCount++;
        auto now = std::chrono::high_resolution_clock::now();
        float dtMs = std::chrono::duration<float, std::milli>(now - s_lastFrameTime).count();
        s_lastFrameTime = now;

        // Skip initial loading frames
        if (s_frameCount < 120) return;

        float thresholdMs = 25.0f; // 25 ms (~40 FPS drop)
        if (Config::get().frame_pacing && Config::get().frame_pacing_target > 0) {
            thresholdMs = (1000.0f / (float)Config::get().frame_pacing_target) * 1.4f;
            if (thresholdMs < 8.0f) thresholdMs = 8.0f;
        }

        if (dtMs > thresholdMs) {
            if (!s_logFile) {
                s_logFile = std::fopen("angle_stutters.log", "a");
            }
            if (s_logFile) {
                std::fprintf(s_logFile, "[STUTTER] Frame #%lu took %.2f ms (threshold: %.2f ms)\n",
                             s_frameCount, dtMs, thresholdMs);
                std::fflush(s_logFile);
            }
        }
    }

    void shutdown() {
        if (s_logFile) {
            std::fclose(s_logFile);
            s_logFile = nullptr;
        }
    }
}

