#include "config.hpp"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

Config& Config::get() {
    static Config c;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        HMODULE self = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&Config::get),
                           &self);
        char path[MAX_PATH] = {};
        if (self && GetModuleFileNameA(self, path, MAX_PATH)) {
            char* slash = strrchr(path, '\\');
            if (slash) {
                *slash = '\0';
                std::string dir = std::string(path) + "\\";
                c.log_file = dir + c.log_file;
                c.shader_cache_dir = dir + c.shader_cache_dir;
                c.plist_cache_dir = dir + c.plist_cache_dir;
            }
        }
    }
    return c;
}

static std::string trim(std::string s) {
    auto notSpace = [](int c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

static bool parseBool(const std::string& v) {
    std::string lower;
    lower.reserve(v.size());
    for (char c : v) lower.push_back((char)std::tolower((unsigned char)c));
    return (lower == "true" || lower == "1" || lower == "yes" || lower == "on");
}

static uint32_t parseHex(const std::string& v) {
    if (v.empty()) return 0;
    try {
        if (v.size() > 2 && (v[0] == '0') && (v[1] == 'x' || v[1] == 'X'))
            return (uint32_t)std::stoul(v.substr(2), nullptr, 16);
        return (uint32_t)std::stoul(v, nullptr, 0);
    } catch (...) {
        return 0;
    }
}

void Config::load(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) return;  // keep defaults

    std::string dir;
    std::string pathStr(path);
    auto lastSlash = pathStr.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        dir = pathStr.substr(0, lastSlash + 1);
    }

    std::string line;
    std::string section;

    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (section == "ANGLE") {
            if      (key == "backend")  backend  = val;
            else if (key == "debug")    debug    = parseBool(val);
            else if (key == "enabled")  enabled  = parseBool(val);
            else if (key == "log_file") {
                if (!dir.empty() && val.find(':') == std::string::npos && val.front() != '/' && val.front() != '\\') {
                    log_file = dir + val;
                } else {
                    log_file = val;
                }
            }
        } else if (section == "Boost") {
            if      (key == "gpu_forcer")     gpu_forcer     = parseBool(val);
            else if (key == "fast_allocator") fast_allocator = parseBool(val);
            else if (key == "timer_fix")      timer_fix      = parseBool(val);
            else if (key == "thread_boost")   thread_boost   = parseBool(val);
            else if (key == "cpu_affinity")   cpu_affinity   = parseHex(val);
            else if (key == "sse_math")       sse_math       = parseBool(val);
            else if (key == "power_boost")    power_boost    = parseBool(val);
        } else if (section == "BoostV2") {
            if      (key == "d3d11_unlock")        d3d11_unlock        = parseBool(val);
            else if (key == "process_perf")        process_perf        = parseBool(val);
            else if (key == "pcore_pin")           pcore_pin           = parseBool(val);
            else if (key == "gpu_residency")       gpu_residency       = parseBool(val);
            else if (key == "stutter_monitor")     stutter_monitor     = parseBool(val);
            else if (key == "no_fs_optimizations") no_fs_optimizations = parseBool(val);
            else if (key == "effect_lod")          effect_lod          = parseBool(val);
            else if (key == "drawcall_budget")     drawcall_budget     = parseBool(val);
            else if (key == "iocp_loader")         iocp_loader         = parseBool(val);
            else if (key == "thread_qos")          thread_qos          = parseBool(val);
        } else if (section == "BoostAdvanced") {
            if      (key == "tex_compress")         tex_compress         = parseBool(val);
            else if (key == "nvapi_profile")        nvapi_profile        = parseBool(val);
            else if (key == "shader_cache")         shader_cache         = parseBool(val);
            else if (key == "shader_cache_dir") {
                if (!dir.empty() && val.find(':') == std::string::npos && val.front() != '/' && val.front() != '\\') {
                    shader_cache_dir = dir + val;
                } else {
                    shader_cache_dir = val;
                }
            }
            else if (key == "large_address_aware")  large_address_aware  = parseBool(val);
            else if (key == "gl_state_dedup")       gl_state_dedup       = parseBool(val);
            else if (key == "working_set_prefetch") working_set_prefetch = parseBool(val);
            else if (key == "fmod_tuning")          fmod_tuning          = parseBool(val);
            else if (key == "fmod_sample_rate")     fmod_sample_rate     = std::atoi(val.c_str());
            else if (key == "async_asset_loader")   async_asset_loader   = parseBool(val);
            else if (key == "async_loader_threads") async_loader_threads = std::atoi(val.c_str());
            else if (key == "force_no_vsync")       force_no_vsync       = parseBool(val);
            else if (key == "precise_sleep")        precise_sleep        = parseBool(val);
            else if (key == "heap_compact_interval")heap_compact_interval= std::atoi(val.c_str());
        } else if (section == "BoostRender") {
            if      (key == "depth_off")         depth_off         = parseBool(val);
            else if (key == "mipmap_off")        mipmap_off        = parseBool(val);
            else if (key == "noop_finish")       noop_finish       = parseBool(val);
            else if (key == "noop_geterror")     noop_geterror     = parseBool(val);
            else if (key == "vbo_pool")          vbo_pool          = parseBool(val);
            else if (key == "vbo_pool_size_mb")  vbo_pool_size_mb  = std::atoi(val.c_str());
            else if (key == "vertex_compress")   vertex_compress   = parseBool(val);
            else if (key == "instancing")        instancing        = parseBool(val);
            else if (key == "dyn_resolution")    dyn_resolution    = parseBool(val);
            else if (key == "dyn_res_target_fps")dyn_res_target_fps= std::atoi(val.c_str());
        } else if (section == "BoostIO") {
            if      (key == "fast_io")        fast_io       = parseBool(val);
            else if (key == "ramdisk_cache")  ramdisk_cache = parseBool(val);
            else if (key == "ramdisk_path")   ramdisk_path  = val;
            else if (key == "loader_cache")   loader_cache  = parseBool(val);
        } else if (section == "BoostCPU") {
            if      (key == "sse_memcpy")    sse_memcpy    = parseBool(val);
            else if (key == "string_intern") string_intern = parseBool(val);
            else if (key == "mimalloc_full") mimalloc_full = parseBool(val);
            else if (key == "silent_debug")  silent_debug  = parseBool(val);
        } else if (section == "BoostSystem") {
            if      (key == "wddm_priority")  wddm_priority  = parseBool(val);
            else if (key == "game_mode")      game_mode      = parseBool(val);
            else if (key == "smart_cpu_pin")  smart_cpu_pin  = parseBool(val);
            else if (key == "mitigation_off") mitigation_off = parseBool(val);
        } else if (section == "BoostLatency") {
            if      (key == "allow_tearing")        allow_tearing        = parseBool(val);
            else if (key == "waitable_swap")        waitable_swap        = parseBool(val);
            else if (key == "frame_pacing")         frame_pacing         = parseBool(val);
            else if (key == "frame_pacing_target")  frame_pacing_target  = std::atoi(val.c_str());
            else if (key == "mmcss_pro_audio")      mmcss_pro_audio      = parseBool(val);
            else if (key == "shader_warmup")        shader_warmup        = parseBool(val);
            else if (key == "low_latency")          low_latency          = parseBool(val);
            else if (key == "gl_no_error")          gl_no_error          = parseBool(val);
            else if (key == "anti_stutter")         anti_stutter         = parseBool(val);
        } else if (section == "BoostGD") {
            if      (key == "skip_intro")       skip_intro       = parseBool(val);
            else if (key == "object_pool")      object_pool      = parseBool(val);
            else if (key == "object_pool_size") object_pool_size = std::atoi(val.c_str());
            else if (key == "plist_binary")     plist_binary     = parseBool(val);
            else if (key == "plist_cache_dir") {
                if (!dir.empty() && val.find(':') == std::string::npos && val.front() != '/' && val.front() != '\\') {
                    plist_cache_dir = dir + val;
                } else {
                    plist_cache_dir = val;
                }
            }
            else if (key == "skip_shake_flash") skip_shake_flash = parseBool(val);
            else if (key == "level_predecode")  level_predecode  = parseBool(val);
            else if (key == "predecode_threads")predecode_threads= std::atoi(val.c_str());
        } else if (section == "BoostRenderAdv") {
            if (key == "atlas_size")      atlas_size      = std::atoi(val.c_str());
            else if (key == "fbo_cache")       fbo_cache       = parseBool(val);
            else if (key == "fbo_pool_size")   fbo_pool_size   = std::atoi(val.c_str());
            else if (key == "triple_buffer")   triple_buffer   = parseBool(val);
            else if (key == "disable_aa")      disable_aa      = parseBool(val);
            else if (key == "blend_optimize")  blend_optimize  = parseBool(val);
        } else if (section == "BoostCocos") {
            if      (key == "particle_throttle") particle_throttle = parseBool(val);
            else if (key == "particle_max")      particle_max      = std::atoi(val.c_str());
            else if (key == "texcache_preload")  texcache_preload  = parseBool(val);
            else if (key == "label_cache")       label_cache       = parseBool(val);
            else if (key == "scheduler_skip")    scheduler_skip    = parseBool(val);
            else if (key == "drawcall_sort")     drawcall_sort     = parseBool(val);
            else if (key == "index_buffer_gen")  index_buffer_gen  = parseBool(val);
        } else if (section == "BoostSysAdv") {
            if      (key == "ftz_daz")       ftz_daz       = parseBool(val);
            else if (key == "spectre_off")   spectre_off   = parseBool(val);
            else if (key == "io_priority")   io_priority   = parseBool(val);
            else if (key == "mem_priority")  mem_priority   = parseBool(val);
            else if (key == "stack_trim")    stack_trim    = parseBool(val);
        } else if (section == "BoostPipeline") {
            if (key == "scissor_tight")    scissor_tight    = parseBool(val);
            else if (key == "vertex_dedup")     vertex_dedup     = parseBool(val);
            else if (key == "batch_coalesce")   batch_coalesce   = parseBool(val);
        } else if (section == "BoostNetwork") {
            if      (key == "dns_prefetch")          dns_prefetch          = parseBool(val);
            else if (key == "http_pool")             http_pool             = parseBool(val);
            else if (key == "online_block_gameplay") online_block_gameplay = parseBool(val);
            else if (key == "server_cache")          server_cache          = parseBool(val);
            else if (key == "winsock_opt")           winsock_opt           = parseBool(val);
        } else if (section == "BoostAudio") {
            if      (key == "fmod_channel_limit") fmod_channel_limit = parseBool(val);
            else if (key == "fmod_max_channels")  fmod_max_channels  = std::atoi(val.c_str());
            else if (key == "fmod_software_mix")  fmod_software_mix  = parseBool(val);
            else if (key == "audio_thread_pin")   audio_thread_pin   = parseBool(val);
            else if (key == "sound_preload")      sound_preload      = parseBool(val);
            else if (key == "audio_ram_compress") audio_ram_compress = parseBool(val);
        } else if (section == "BoostExtreme") {
            if      (key == "etw_disable")     etw_disable     = parseBool(val);
            else if (key == "wer_disable")     wer_disable     = parseBool(val);
            else if (key == "smartscreen_off") smartscreen_off = parseBool(val);
            else if (key == "numa_aware")      numa_aware      = parseBool(val);
            else if (key == "huge_pages")      huge_pages      = parseBool(val);
            else if (key == "prefetcher_off")  prefetcher_off  = parseBool(val);
            else if (key == "workingset_lock") workingset_lock = parseBool(val);
            else if (key == "gpu_thread_prio") gpu_thread_prio = parseBool(val);
            else if (key == "present_skip_idle") present_skip_idle = parseBool(val);
        } else if (section == "HUD") {
            if      (key == "enabled")         hud_enabled     = parseBool(val);
            else if (key == "show_fps")        show_fps        = parseBool(val);
            else if (key == "show_drawcalls")  show_drawcalls  = parseBool(val);
            else if (key == "show_frame_time") show_frame_time = parseBool(val);
        } else if (section == "Megahack") {
            if      (key == "megahack_detected")        megahack_detected        = parseBool(val);
            else if (key == "mod_loader_detected")      mod_loader_detected      = parseBool(val);
            else if (key == "megahack_force_fbo0_on_swap") megahack_force_fbo0_on_swap = parseBool(val);
        }
    }
}

bool Config::isVulkanBackend() const {
    return backend == "vulkan";
}

void Config::applyBackendSafetyOverrides() {
    if (!isVulkanBackend()) return;

    // ANGLE's Vulkan backend is much less forgiving around effect-heavy
    // render-to-texture paths. Keep validation/synchronization intact and
    // disable experimental pipeline rewrites even if an older ini enables them.
    gl_no_error = false;
    noop_finish = false;
    noop_geterror = false;

    fbo_cache = false;

    scissor_tight = false;
    vertex_dedup = false;
    batch_coalesce = false;
}
