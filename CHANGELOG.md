# Changelog

All notable changes to ReviANGLE fork are documented in this file.

**Fork Versioning Note:**
- Versions 1.x represent active fork maintenance as a separate branch from Reviusion/ReviANGLE
- Fork versions start at v1.0.0 (independently versioned)
- Vulkan backend is exclusive to this fork; DirectX 11 backend has been improved

---

## [1.2.0] - 2026-09-04

### Added & Fixed in v1.2.0
- **Frame Rate Stability & Micro-Stutter Fixes**:
  - Disabled internal frame pacing conflicts by default (`frame_pacing_target = 0` treated as uncapped/disabled) to prevent fighting FPS bypass.
  - Replaced CPU spin-wait loops in `boost_sleep.cpp` with zero-CPU high-resolution kernel waitable timers (`CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`), eliminating 100% CPU core pinning on short sleep cycles.
  - Balanced process and thread priorities from `TIME_CRITICAL` to `ABOVE_NORMAL` to prevent starving GPU driver command submission threads, DWM, and FMOD audio.
- **Performance Defaults & Latency Optimization**:
  - Updated all pre-tuned configuration files to use safe non-conflicting defaults (`allow_tearing=true`, `force_no_vsync=true`, `frame_pacing=false`, `precise_sleep=false`, `low_latency=false`).
- **Windows DirectX 11 vs. Vulkan Performance Notice**:
  - Added documentation and configuration notes explaining that **DirectX 11 (`backend=d3d11`) provides significantly higher peak FPS and lower input latency on Windows (especially NVIDIA GPUs)** compared to Vulkan.
  - DirectX 11 supports hardware-level DXGI Allow Tearing (`DXGI_PRESENT_ALLOW_TEARING` + Flip Model), bypassing DWM frame queues with minimal CPU translation overhead. Vulkan lacks DXGI swap chains under Windows and incurs additional CPU command buffer translation costs in ANGLE, resulting in ~200-300+ FPS lower throughput at extreme uncapped frame rates.

---

## [1.1.0] - 2026-09-03

### Added & Fixed in v1.1.0
- **Mod Compatibility**:
  - Megahack now works!
- **Automated CI/CD Release Pipeline**:
  - Created `.github/workflows/release.yml` to automatically compile both DirectX 11 and Vulkan backends from source and publish release ZIP archives on tag creation (`v*`).
  - Restructured third-party dependencies into `deps/dx11` and `deps/vulkan` with full provenance documentation in `deps/README.md`.
- **Local Release Packaging Script**:
  - Created `build_release.ps1` PowerShell helper script to automate clean local compilation, staging, and ZIP packaging for release.
- **Updated Documentation**:
  - Completely updated `README.md`, `docs/BUILDING.md`, `docs/INSTALLATION.md`, and `FORK_INFO.md` with accurate CMake build flags (`-DREVIANGLE_BACKEND_D3D11=ON` / `-DREVIANGLE_BACKEND_VULKAN=ON`), script usage, and release archive structure.

---

## [1.0.0] - 2026-06-02

### Changes in v1.0.0
- **Dual-backend support**: DirectX 11 (default, most compatible) and Vulkan (modern GPUs)
- **Professional uninstaller**: GUI-based ReviANGLE-Uninstall.exe with auto-detection and cleanup
- **ReviANGLE Studio**: Dear ImGui-based configuration editor for real-time settings
- **84 optimization modules**: ANGLE hot-path hooks for:
  - Frame pacing and DXGI low-latency present
  - GPU thread priority and driver profile management
  - Working-set memory lock for frame-time stability
  - Optional half-resolution rendering for maximum throughput
- **Forked from Reviusion/ReviANGLE** (v2.x series) with independent versioning strategy
- **Hardware-tested**: DirectX 11 on legacy (i5-3230M + GT 630M, by Reviusion) and modern (RTX 2080 Super + Ryzen 5 3600)
- **Both backends confirmed working**: Vulkan 200+ FPS on modern hardware, DirectX 11 baseline 88 FPS

---

## Version Independence Note

This fork uses independent v1.x versioning starting from v1.0.0

Fork versioning strategy:
- **v1.0.0** = Fork release with dual-backend support (DirectX 11 + Vulkan), uninstaller, optimization modules
- **v1.1.0** = MegaHack compatibility fixes, WGL dummy context fix for Vulkan, automated CI/CD pipeline & packaging
- **v1.x++** = Future enhancements and upstream tracking
- Separate from upstream to avoid version conflicts
- Vulkan as first-class backend (fork-exclusive)
- Active maintenance as independent branch

For upstream history, see [Reviusion/ReviANGLE](https://github.com/Reviusion/ReviANGLE/releases).
