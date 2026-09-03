#pragma once
#include <cstdint>
#include <string>

struct Config {
  // ANGLE
#ifdef REVIANGLE_BACKEND_VULKAN
  std::string backend = "vulkan";
#else
  std::string backend = "d3d11";
#endif
  bool debug = false;
  bool enabled = true;
  std::string log_file = "angle_log.txt";

  // Boost (base modules)
  bool gpu_forcer = true;
  bool fast_allocator = true;
  bool timer_fix = true;
  bool thread_boost = true;
  uint32_t cpu_affinity = 0;
  bool sse_math = true;
  bool power_boost = true;

  // BoostAdvanced
  bool tex_compress = true;
  bool nvapi_profile = true;
  bool shader_cache = true;
  std::string shader_cache_dir = "shader_cache";
  bool large_address_aware = true;
  bool gl_state_dedup = true;
  bool working_set_prefetch = true;
  bool fmod_tuning = true;
  int fmod_sample_rate = 44100;
  bool async_asset_loader = true;
  int async_loader_threads = 4;
  bool force_no_vsync = true;
  bool precise_sleep = true;
  int heap_compact_interval = 30;

  // BoostRender
  bool depth_off = false;
  bool mipmap_off =
      false; // safer default - some shaders rely on mipmap sampling
  // Aggressive perf - masks OpenGL errors / forces pipeline behavior. Off by
  // default for stability.
  bool noop_finish = false; // make glFinish a no-op (skip pipeline stalls)
  bool noop_geterror =
      false; // glGetError -> GL_NO_ERROR (skip driver round-trip)
  bool vbo_pool = true;
  int vbo_pool_size_mb = 16;
  bool vertex_compress = false; // risky by default
  bool instancing = true;
  bool dyn_resolution = false;
  int dyn_res_target_fps = 60;

  // BoostIO
  bool fast_io = true;
  bool ramdisk_cache = false;
  std::string ramdisk_path = "";
  bool loader_cache = true;

  // BoostCPU
  bool sse_memcpy = true;
  bool string_intern = true;
  bool mimalloc_full = true;
  bool silent_debug = true;

  // BoostSystem
  bool wddm_priority = true;
  bool game_mode = true;
  bool smart_cpu_pin = true;
  bool mitigation_off = false; // security impact

  // BoostLatency
  bool allow_tearing = true;
  bool waitable_swap = true;
  bool frame_pacing = true;
  int frame_pacing_target =
      0; // 0 = auto-detect monitor refresh; >0 = forced FPS cap
  bool mmcss_pro_audio = true;
  bool shader_warmup = false; // off by default - can crash on invalid shaders
  bool low_latency = true;
  bool gl_no_error =
      true; // EGL_CONTEXT_OPENGL_NO_ERROR_KHR - kills per-call ANGLE validation
  bool anti_stutter = true; // disable affinity auto-update + EcoQoS thread
                            // throttling for jitter

  // v3.0.0 modules
  bool d3d11_unlock = true;
  bool process_perf = true;
  bool pcore_pin = true;
  bool gpu_residency = true;
  bool stutter_monitor = false;
  bool no_fs_optimizations = true;
  bool effect_lod = true;
  bool drawcall_budget = true;
  bool iocp_loader = true;
  bool thread_qos = true;

  // BoostGD
  bool skip_intro = true;
  bool object_pool = true;
  int object_pool_size = 4096;
  bool plist_binary = true;
  std::string plist_cache_dir = "plist_cache";
  bool skip_shake_flash = true;
  bool level_predecode = true;
  int predecode_threads = 2;
  // BoostRenderAdv
  int atlas_size = 2048;
  bool fbo_cache = true;
  int fbo_pool_size = 8;
  bool triple_buffer = true;
  bool disable_aa = true;
  bool blend_optimize = true;

  // BoostCocos
  bool particle_throttle = true;
  int particle_max = 150;
  bool texcache_preload = true;
  bool label_cache = true;
  bool scheduler_skip = true;
  bool drawcall_sort = true;
  bool index_buffer_gen = true;

  // BoostSysAdv
  bool ftz_daz = true;
  bool spectre_off = false; // security impact
  bool io_priority = true;
  bool mem_priority = true;
  bool stack_trim = true;

  // BoostPipeline
  bool scissor_tight = false; // can clip sprites
  bool vertex_dedup = true;
  bool batch_coalesce = true;

  // BoostNetwork
  bool dns_prefetch = true;
  bool http_pool = true;
  bool online_block_gameplay = true;
  bool server_cache = true;
  bool winsock_opt = true;

  // BoostAudio
  bool fmod_channel_limit = true;
  int fmod_max_channels = 16;
  bool fmod_software_mix = true;
  bool audio_thread_pin = true;
  bool sound_preload = true;
  bool audio_ram_compress = false;

  // BoostExtreme
  bool etw_disable = true;
  bool wer_disable = true;
  bool smartscreen_off = true;
  bool numa_aware = true;
  bool huge_pages = false;     // needs SeLockMemoryPrivilege
  bool prefetcher_off = false; // needs admin

  // BoostExtreme - additional Windows-level perf wins
  bool workingset_lock =
      false; // hard-pin min working set so OS can't page us out
  bool gpu_thread_prio = true; // IDXGIDevice::SetGPUThreadPriority(+7)

  // Optional modules
  bool present_skip_idle = false;

  // Megahack support
  bool megahack_detected = false;
  bool mod_loader_detected = false;
  bool megahack_force_fbo0_on_swap = false;

  static Config &get();
  void load(const char *path);
  bool isVulkanBackend() const;
  void applyBackendSafetyOverrides();
  bool shouldSkipGrabPasses() const { return false; }
};
