# ReviANGLE Architecture

This document explains how ReviANGLE works internally. Useful if you want to contribute, port to a different game, or just understand why your FPS went up.

## High-level

```
                            ┌────────────────────────┐
   GeometryDash.exe ───────▶│   opengl32.dll         │  ← ReviANGLE proxy
   (calls glXXX functions)  │   (gl_proxy.cpp,       │
                            │    gl_proxy_ext.cpp)   │
                            └────────────┬───────────┘
                                         │  forwards (with dedup)
                                         ▼
                            ┌────────────────────────┐
                            │   libGLESv2.dll        │  ← ANGLE
                            │   (Google's            │
                            │    GLES → D3D11        │
                            │    translator)         │
                            └────────────┬───────────┘
                                         │
                                         ▼
                            ┌────────────────────────┐
                            │   d3d11.dll            │  ← Microsoft DirectX 11
                            └────────────┬───────────┘
                                         │
                                         ▼
                            ┌────────────────────────┐
                            │   nvlddmkm.sys / etc.  │  ← GPU driver
                            └────────────────────────┘
```

Plus, the proxy has 80+ side-modules (`boost_*.cpp`) that hook into Win32 / D3D11 / NVAPI to add system-level optimizations.

## Module categories

| Category | Examples | What they do |
|----------|----------|--------------|
| **Init / wiring** | `dllmain.cpp`, `angle_loader.cpp`, `wgl_proxy.cpp` | Bootstrap the proxy, load ANGLE DLLs, bridge WGL → EGL |
| **GL state dedup** | `gl_proxy.cpp`, `gl_proxy_ext.cpp` | Skip redundant GL state changes (texture, blend, uniforms, …) |
| **Frame pacing** | `boost_frame_pacing.cpp`, `boost_low_latency.cpp` | High-res waitable timer + DXGI 1-frame latency |
| **GPU policy** | `boost_nvapi.cpp`, `boost_gpu_forcer.cpp`, `boost_wddm_prio.cpp` | Tell driver "max perf, no vsync, GD is foreground" |
| **OS scheduling** | `boost_thread.cpp`, `boost_mmcss.cpp`, `boost_anti_stutter.cpp`, `boost_workingset_lock.cpp` | Pin threads, raise priority, lock RAM pages |
| **Render tweaks** | `boost_no_aa.cpp`, `boost_mipmap_off.cpp`, `boost_blend_opt.cpp` | Drop MSAA, skip mipmaps, optimize blends |
| **Cocos2d-x specific** | `boost_object_pool.cpp`, `boost_trigger_cache.cpp`, `boost_skip_intro.cpp` | Hook GD-specific hot paths |

## Initialization sequence

1. **GD process starts**, Windows resolves `opengl32.dll` from the GD directory (current dir is searched before system32 because of GD's manifest).
2. **`DllMain DLL_PROCESS_ATTACH`** fires:
   - `Config::get().load("angle_config.ini")` reads our config.
   - All "non-GL-dependent" boost modules are applied (`boost_timer`, `boost_thread`, `boost_power`, `boost_workingset_lock`, …).
   - Non-GL-dependent boost modules (e.g. `boost_nvapi`) run here.
3. **GD calls `wglCreateContext`** → routed to our `wgl_glCreateContext`:
   - We create an EGL context via ANGLE.
   - We call `boost_frame_pacing::apply()`, `boost_allow_tearing::apply()`, `boost_low_latency::apply()`, etc. - these modules need a live D3D11 device.
4. **GD calls `wglMakeCurrent`** → we call `eglMakeCurrent`. After success, GL is fully ready.
5. From here on, every GL call from GD goes through our proxy (`gl_glDrawArrays`, `gl_glBindTexture`, etc.) → dedup logic → forward to ANGLE.

## State dedup design

cocos2d-x re-sets identical render state on every sprite. Our proxy intercepts state-change calls and skips them when the new value matches the current one. Storage:

- **Per-thread** (`thread_local`) - most state is per-context, contexts are bound to threads.
- **Direct-mapped hash cache** for uniforms - keyed by `(program, location)`, 32 entries × small payload. Lock-free, no allocations.
- **Bitmask** for boolean state (e.g. `glEnableVertexAttribArray`).

### Coverage matrix

The "passthrough" column shows what was untouched in vanilla GD, "deduped" what we now skip when redundant.

| GL function | Storage | Pre-mod | ReviANGLE |
|-------------|---------|--------|-----|
| `glActiveTexture` | thread_local | passthrough | dedup |
| `glBindTexture` | thread_local per-target | passthrough | dedup |
| `glBindBuffer` | thread_local 8-target | passthrough | **dedup** |
| `glBindFramebuffer` | thread_local 2-target | passthrough | **dedup** |
| `glBindVertexArray` | thread_local | passthrough | **dedup** |
| `glEnable` / `glDisable` | thread_local bitmask | passthrough | dedup |
| `glColorMask` | thread_local nibble | passthrough | dedup |
| `glDepthMask` / `glDepthFunc` | thread_local | passthrough | dedup |
| `glStencilFunc` / `glStencilMask` / `glStencilOp` | thread_local | passthrough | dedup |
| `glCullFace` / `glFrontFace` | thread_local | passthrough | dedup |
| `glPolygonOffset` / `glScissor` / `glLineWidth` | thread_local | passthrough | **dedup** |
| `glHint` | thread_local 256-slot | passthrough | **dedup** |
| `glClear` (smart) | + `markDirty` | passthrough | **dedup** |
| `glClearColor` / `glClearDepthf` / `glClearStencil` | thread_local | passthrough | **dedup** |
| `glBlendFunc` / `glBlendEquation` / `glBlendFuncSeparate` | thread_local | passthrough | dedup |
| `glUseProgram` | thread_local | passthrough | dedup |
| `glUniform{1,2,3,4}f` | per-program/loc cache | passthrough | dedup |
| `glUniform{1,2}i` | per-program/loc cache | passthrough | dedup |
| `glUniform{1,2,3,4}fv (count==1)` | per-program/loc cache | passthrough | dedup |
| `glUniform{1,2,4}iv (count==1)` | per-program/loc cache | passthrough | dedup |
| `glUniformMatrix{3,4}fv (count==1)` | per-program/loc cache | passthrough | dedup |
| `glEnableVertexAttribArray` / `glDisableVertexAttribArray` | thread_local bitmask | passthrough | **dedup** |

Total: ~46 functions deduped.

## Why dedup helps so much on cocos2d

cocos2d-x renders each sprite via:
1. `glBindTexture(spriteTexture)`
2. `glUseProgram(spriteProgram)`
3. `glUniform4f(u_color, r, g, b, a)`
4. `glUniformMatrix4fv(u_mvp, …)`
5. `glEnableVertexAttribArray(0, 1, 2)`
6. `glVertexAttribPointer(0, 1, 2, …)`
7. `glDrawElements(GL_TRIANGLES, 6, …)`

For 1000 sprites in a frame using the same atlas + same shader + same color, that's:
- 1000 × 5 redundant calls = **5000 calls eliminated** by dedup
- Each call costs ~1-3 µs of CPU (driver validation + ANGLE translation)
- → **~5-15 ms saved per frame** on a 2-core CPU = **+30-100 % FPS** in CPU-bound scenes

## Frame pacing detail

`boost_frame_pacing.cpp` implements a precise FPS limiter:

1. After every frame, compute `elapsed_since_last_present`.
2. If `elapsed < target_dt`:
   - Phase 1: block on a high-resolution waitable timer (`CreateWaitableTimerExW(HIGH_RESOLUTION)`) for the bulk of the wait - **0 % CPU**, ~100 µs precision.
   - Phase 2: tight `YieldProcessor` spin for the last ~200 µs to hit sub-100 µs precision.
3. Anchor `last_present` to the ideal tick - long-term FPS stays exact, no cumulative drift.
4. If frame already exceeded target (GPU/CPU bound), anchor to `now` to avoid negative drift.

The high-res timer is **the** key win on 2-core CPUs - the previous Sleep+spin approach burned a whole core busy-waiting, which directly competed with cocos2d's main thread.

## NVAPI DRS - what we set

`boost_nvapi.cpp` opens an NVAPI DRS session, finds (or falls back to global profile), and sets:

| Setting | Value | Why |
|---------|-------|-----|
| `PREFERRED_PSTATE` | `PREFER_MAX` (P0) | Pin GPU at max clock - no idle ramp-up jitter |
| `POWER_MIZER_LEVEL_AC` | `MAX_PERF` | Driver power policy = max |
| `VSYNC_MODE` | `FORCE_OFF` | Driver-level vsync killed |
| `OGL_THREADED_OPTIMIZATION` | `ENABLE` | Driver multi-threaded submission |
| `FRAME_LIMITER` | `OFF` | Driver-imposed FPS cap killed |

These persist in the user's Nvidia profile DB.

## Memory & threading model

- All boost modules are **single-threaded**, applied during DLL init or postGLInit.
- All hot-path caches (state, uniforms) are `thread_local` - no contention.
- No exceptions thrown anywhere on the GL hot path.
- No heap allocations on the GL hot path (caches are statically sized).
- No virtual calls on the GL hot path.

## Limits / non-goals

ReviANGLE is intentionally **not**:
- A graphics improvement mod (no FXAA, no enhanced effects, no HDR).
- A trainer / cheat (no level skip, no speedhack).
- A general OpenGL→D3D shim usable for other apps (cocos2d-x assumptions are baked in).

## Code style summary

- **C++17**, MSVC-only.
- 4-space indent.
- One module = one `boost_*.cpp` file, kept small.
- Comments explain **why**, in English.
- No `using namespace std;` at file scope.
- All hot-path code is `noexcept`-equivalent (no throw, no allocation).
- Public symbols are prefixed `gl_gl*` (proxy exports), `gdangle_*` (internal helpers), or `boost_<module>::*` (per-module API).
