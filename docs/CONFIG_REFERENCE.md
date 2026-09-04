# `angle_config.ini` reference

The full reference of every option in `angle_config.ini`. For interactive editing, run `gd-angle-editor.exe` - it has the same info as a UI with bilingual descriptions.

> **Note**: The shipped `angle_config.ini` already has full bilingual comments inline. This document is a flat summary for browsing.

## Section: `[ANGLE]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `backend` | enum | `d3d11` | ANGLE backend: `d3d11` / `d3d9` / `vulkan`. D3D11 recommended. D3D9 fallback for old drivers. |
| `debug` | bool | `false` | Verbose log to `angle_log.txt`. Costs ~5 % perf. Turn on only when debugging. |
| `log_file` | string | `angle_log.txt` | Log file path (relative to GD exe). |

## Section: `[Boost]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `gpu_forcer` | bool | `true` | `NvOptimusEnablement = 1` export - Optimus laptops use dGPU instead of iGPU. |
| `fast_allocator` | bool | `true` | `HeapSetInformation` to enable LFH on process heap. |
| `timer_fix` | bool | `true` | `timeBeginPeriod(1)` for 1 ms scheduler granularity. |
| `thread_boost` | bool | `true` | `SetThreadPriority(THREAD_PRIORITY_ABOVE_NORMAL)`. |
| `cpu_affinity` | hex | `0` | If non-zero, pin GD to specific cores. `0xC` = cores 2-3. `0` = all. |
| `sse_math` | bool | `true` | Set FPU rounding to truncate (faster int conversions). |
| `power_boost` | bool | `true` | `PROCESS_POWER_THROTTLING_EXECUTION_SPEED=0` - disable Win10 EcoQoS throttle. |

## Section: `[BoostAdvanced]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `tex_compress` | bool | `false` | Compress RGBA8 → DXT1 on upload. **Risky** on FL9 path. |
| `nvapi_profile` | bool | `true` | Real NVAPI DRS settings (PSTATE=P0, POWER_MIZER=MAX, etc.). Silent no-op on non-Nvidia. |
| `shader_cache` | bool | `true` | Disk-cache compiled shaders. |
| `shader_cache_dir` | string | `shader_cache` | Where to put cache files. |
| `large_address_aware` | bool | `true` | Set LAA flag on running process via PE patching. |
| `gl_state_dedup` | bool | `true` | (Legacy module - actual dedup is in `gl_proxy.cpp`, this flag mostly cosmetic.) |
| `working_set_prefetch` | bool | `false` | Prefault GD's code pages. **Causes init stalls** on test hardware. |
| `fmod_tuning` | bool | `false` | FMOD audio tuning. **IAT hook on FMOD** can crash. |
| `fmod_sample_rate` | int | `44100` | FMOD sample rate when `fmod_tuning=true`. |
| `async_asset_loader` | bool | `false` | Parallel asset loader. **2-core CPUs** see no benefit. |
| `async_loader_threads` | int | `4` | Worker count when `async_asset_loader=true`. |
| `force_no_vsync` | bool | `true` | `eglSwapInterval(0)` - disable vsync at EGL level. |
| `precise_sleep` | bool | `true` | High-resolution `Sleep` patch via `NtSetTimerResolution`. |
| `heap_compact_interval` | int | `30` | Heap-compaction interval in seconds. |
| `d3d11_multithread` | bool | `false` | D3D11 multithread protection. **Conflicts** with ANGLE's own threading. |

## Section: `[BoostRender]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `depth_off` | bool | `false` | Skip depth test/clear. **Breaks triggers** in GD. |
| `mipmap_off` | bool | `true` | Skip mipmap chain generation. Good on weak GPUs. |
| `noop_finish` | bool | `false` | Make `glFinish` a no-op (skip pipeline stalls). Aggressive. |
| `noop_geterror` | bool | `false` | `glGetError` → 0 (skip driver round-trip). Aggressive. |
| `vbo_pool` | bool | `false` | VBO pooling. **Conflicts** with cocos2d's batcher → jitter. |
| `vbo_pool_size_mb` | int | `16` | VBO pool size when enabled. |
| `vertex_compress` | bool | `false` | FP32 → FP16 vertex format. **Risky** on text/UI. |
| `instancing` | bool | `false` | `glDrawElementsInstanced` batching. **Mismatches** cocos2d vertex layout. |
| `dyn_resolution` | bool | `false` | Dynamic resolution scaling on FPS dips. **Causes visible jitter**. |
| `dyn_res_target_fps` | int | `60` | Target FPS for dyn-res. |

## Section: `[BoostIO]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `fast_io` | bool | `true` | `SetFileInformationByHandle` to disable AV scan on hot files. |
| `ramdisk_cache` | bool | `false` | Copy `Resources/` to RAM-disk. **Needs ~500 MB free RAM**. |
| `ramdisk_path` | string | `` | RAM-disk path when enabled. |
| `loader_cache` | bool | `true` | Cache `GetProcAddress` / `wglGetProcAddress` lookups. |

## Section: `[BoostCPU]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `sse_memcpy` | bool | `false` | SSE2 memcpy via IAT hook. **CRT mismatch crash risk**. |
| `scene_bvh` | bool | `false` | Bounding volume hierarchy for scene culling. **Experimental**. |
| `string_intern` | bool | `false` | `std::string` interning via IAT hook. **ABI variation crash risk**. |
| `mimalloc_full` | bool | `false` | Replace `new`/`delete` with mimalloc. **DLL not bundled**. |
| `silent_debug` | bool | `true` | Silence `printf` / `OutputDebugString` from cocos2d. |

## Section: `[BoostSystem]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `wddm_priority` | bool | `true` | WDDM context priority HIGH. |
| `game_mode` | bool | `true` | Win10 Game Mode enable + Game Bar disable. |
| `smart_cpu_pin` | bool | `true` | Pin main thread to physical core 0 (avoid HT sibling). |
| `mitigation_off` | bool | `false` | Disable CFG/CET/strict ASLR. **Security trade-off**. |

## Section: `[BoostLatency]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `allow_tearing` | bool | `true` | DXGI ALLOW_TEARING flag (D3D11 flip-model). |
| `waitable_swap` | bool | `true` | Use `WaitForSingleObjectEx` instead of blocking Present. |
| `frame_pacing` | bool | `true` | High-res-timer frame pacer. **Best feel preset**. |
| `frame_pacing_target` | int | `120` | FPS cap. **Set BELOW your worst-case FPS during effects** for max smoothness. `0` = auto-detect monitor refresh. |
| `mmcss_pro_audio` | bool | `true` | MMCSS Pro Audio class - 1 ms scheduling. |
| `shader_warmup` | bool | `false` | Pre-compile linked programs at startup via `glValidateProgram` to force ANGLE D3D11 deferred HLSL JIT. Reduces first-use shader stutter. |
| `low_latency` | bool | `true` | `IDXGIDevice1::SetMaximumFrameLatency(1)` - input lag −33 ms at 60 FPS. |
| `gl_no_error` | bool | `true` | `EGL_CONTEXT_OPENGL_NO_ERROR_KHR` - kills per-call ANGLE validation. |
| `unlock_fps_cap` | bool | `true` | Hook `CCApplication::setAnimationInterval` to remove cocos2d 60 FPS cap. |
| `anti_stutter` | bool | `true` | Disable affinity auto-update + EcoQoS thread throttling. |

## Section: `[BoostGD]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `skip_intro` | bool | `false` | Skip splash screen. **File-hook is risky** for saves. |
| `object_pool` | bool | `false` | Object pool for cocos2d nodes. **Vtable corruption risk**. |
| `object_pool_size` | int | `4096` | Pool size when enabled. |
| `trigger_cache` | bool | `false` | Cache trigger evaluation. **Stale on moving triggers**. |
| `plist_binary` | bool | `false` | Binary plist cache. **Save corruption risk on bad write**. |
| `plist_cache_dir` | string | `plist_cache` | Cache dir when enabled. |
| `skip_shake_flash` | bool | `false` | Skip shake/flash effects. **Heuristic hooks are glitchy**. |
| `level_predecode` | bool | `false` | Pre-decode level on load. **Only 2 CPU threads** = no benefit. |
| `predecode_threads` | int | `2` | Threads for level predecode. |

## Section: `[BoostRenderAdv]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `atlas_merge` | bool | `false` | Merge texture atlases. **Breaks UVs in custom levels**. |
| `atlas_size` | int | `2048` | Max atlas dimension. |
| `frustum_cull` | bool | `false` | Frustum cull cocos2d nodes. **Clips visible objects** (bad bounding boxes). |
| `fbo_cache` | bool | `false` | FBO pool. **Jitter source** (RT-switch complexity). |
| `fbo_pool_size` | int | `8` | FBO pool size. |
| `triple_buffer` | bool | `false` | Triple buffering. **ANGLE ignores** the hint, +1 frame lag. |
| `disable_aa` | bool | `true` | `glDisable(GL_MULTISAMPLE)` - huge win on weak GPUs. |
| `blend_optimize` | bool | `false` | Blend mode optimizer. **Breaks fade transitions**. |

## Section: `[BoostCocos]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `particle_throttle` | bool | `false` | Cap particle count. Visual change. |
| `particle_max` | int | `300` | Max particles per emitter when enabled. |
| `texcache_preload` | bool | `false` | Pre-warm texture cache. **Adds ~3 s startup** for no in-game benefit. |
| `batch_force` | bool | `false` | Force-batch all sprites. **Causes sprite glitches**. |
| `label_cache` | bool | `false` | Cache CCLabel-rendered glyphs. Marginal gain. |
| `scheduler_skip` | bool | `true` | Skip inactive timers in CCScheduler tick. **Free perf**. |
| `drawcall_sort` | bool | `false` | Sort draw calls by texture. **Breaks z-order** in editor. |
| `index_buffer_gen` | bool | `false` | Generate index buffers for batched sprites. **Layout mismatch**. |

## Section: `[BoostSysAdv]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `ftz_daz` | bool | `false` | Set FTZ/DAZ FPU flags. **Affects physics** in subtle ways. |
| `spectre_off` | bool | `false` | Disable Spectre/Meltdown mitigations for this process. **Security trade-off**. |
| `io_priority` | bool | `true` | `NtSetInformationProcess(IoPriority=HIGH)`. |
| `mem_priority` | bool | `true` | `SetProcessInformation(MemoryPriority=HIGH)`. |
| `stack_trim` | bool | `false` | Trim main thread stack to 256 KB. **Recursion crash risk**. |

## Section: `[BoostPipeline]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `pipe_drawsort` | bool | `false` | Pipeline draw-sort. Hot-path hook → jitter on 2-core. |
| `scissor_tight` | bool | `false` | Tighten scissor rect. **Can clip sprites**. |
| `vertex_dedup` | bool | `false` | Dedup vertex uploads. Hot-path hook → jitter. |
| `halfres_effects` | bool | `false` | Half-res particle/effect rendering. Visible quality drop. |
| `shader_simplify` | bool | `false` | Simplify shaders. Visual change. |
| `batch_coalesce` | bool | `false` | Coalesce adjacent draw calls. Currently dead code (roadmap). |

## Section: `[BoostNetwork]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `dns_prefetch` | bool | `true` | Pre-resolve GD's API DNS at startup. |
| `http_pool` | bool | `true` | HTTP connection pool. |
| `online_block_gameplay` | bool | `true` | Block network during gameplay. **Disables online features** (daily, gauntlets). Set false to restore. |
| `server_cache` | bool | `true` | Cache GET responses (server lists, level previews) for 60 s. |
| `winsock_opt` | bool | `true` | Tune SO_SNDBUF/SO_RCVBUF, disable Nagle. |

## Section: `[BoostAudio]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `fmod_channel_limit` | bool | `false` | Limit FMOD active channels. **Can mute legit SFX**. |
| `fmod_max_channels` | int | `16` | Channel cap when enabled. |
| `fmod_software_mix` | bool | `false` | Force FMOD software mixing. **Loses GPU audio offload**. |
| `audio_thread_pin` | bool | `false` | Pin FMOD thread to dedicated core. **2-core CPU starves main**. |
| `sound_preload` | bool | `false` | Preload all `.mp3`/`.ogg`. **RAM-heavy**. |
| `audio_ram_compress` | bool | `false` | RAM-compress audio buffers. **CPU cost > gain**. |

## Section: `[BoostExtreme]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `etw_disable` | bool | `true` | Disable ETW trace hooks for this process. |
| `wer_disable` | bool | `true` | Disable Windows Error Reporting (no Watson dialog on crash). |
| `smartscreen_off` | bool | `true` | Disable SmartScreen for our process. |
| `numa_aware` | bool | `true` | Bind to local NUMA node. No-op on single-NUMA. |
| `huge_pages` | bool | `false` | 2 MB large pages. **Needs `SeLockMemoryPrivilege` (admin)**. |
| `prefetcher_off` | bool | `false` | Disable Win Superfetch/Prefetcher. **Needs admin**. |
| **`workingset_lock`** | **bool** | **`false`** | **Hard-pin min working set (`SetProcessWorkingSetSizeEx`). Unnecessary on 64-bit GD.** |
| **`gpu_thread_prio`** | **bool** | **`true`** | **`IDXGIDevice::SetGPUThreadPriority(+7)`. Boost driver's GPU command-list submission thread.** |
| **`present_skip_idle`** | **bool** | **`false`** | **(opt-in) Skip `eglSwapBuffers` on frames with zero draw calls (idle menus, paused level). Cap of 4 consecutive skips bounds input latency. Saves GPU power on idle scenes.** |
| **`halfres_render`** | **bool** | **`false`** | **(opt-in) Render entire game at half resolution (W/2 × H/2) to an offscreen FBO, upscale via `glBlitFramebuffer` + `GL_LINEAR` on Present. ~30-50% GPU savings on weak GPUs. Visible quality drop on UI text.** |

---

For full bilingual descriptions, see comments inline in the shipped `angle_config.ini`, or use `gd-angle-editor.exe`.
