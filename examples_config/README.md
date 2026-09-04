# Example configurations

> **Note:** Some configs are outdated (taken from og repo) and may not work (most visible for Vulkan backend)
> Every preset here (excluding angle_config.vulkan-stable) uses dx11 backend

Pre-tuned `angle_config.ini` files for different use cases. Copy one of them next to `GeometryDash.exe` (renaming to `angle_config.ini`).

## `angle_config.default-safe.ini`

**Most users should start here.**

- All risky / experimental optimizations turned **off**.
- Frame pacing **on** with `target=0` (auto-detect monitor refresh).
- Online features **enabled** (`online_block_gameplay=false`).
- Should not crash on any reasonable Windows + GPU combo.

When to use: **first time installing**, on a hardware combo that hasn't been tested by the developer, or if any other config crashes.

## `angle_config.best-feel-gt630m.ini`

Tuned for the developer's hardware: **Intel i5-3230M (Ivy Bridge, 2C/4T)** + **NVIDIA GT 630M (Kepler)** + **8 GB RAM** + **90 Hz display** + **Windows 10 22H2**.

- All "safe" optimizations **on**.
- Frame pacing **on** with `target=120` - 8.3 ms budget per frame, fits worst-case effects on this hardware.
- Online features **blocked during gameplay** (`online_block_gameplay=true`) for stability.
- A handful of "risky" tweaks **on** (`tex_compress`, `vbo_pool`, etc.) that are stable on the tested hardware but may not be on yours.

When to use: similar hardware vintage (anything weak, ~2010-2014 era laptop). Test on a known level you've played before; if you see new visual glitches, fall back to `default-safe`.

## `angle_config.benchmark.ini`

Same as `best-feel`, but:
- `frame_pacing=false` - no FPS cap, see your true uncapped FPS.
- `debug=true` - verbose log to `angle_log.txt` for measurement.

When to use: **benchmarking**. Run a level a few times, check `angle_log.txt` for FPS counters, then go back to `best-feel` once you've found your worst-case FPS for picking `frame_pacing_target`.

---

## Picking the right `frame_pacing_target` for your hardware

The single most-important config knob, and the one that needs personalization. Steps:

1. Use `angle_config.benchmark.ini` (pacing OFF, debug ON).
2. Play a few hard levels you know.
3. Watch the FPS counter (or check `angle_log.txt` for `Drawelements per frame` lines).
4. Find your **worst-case FPS during effects**. Call it `Wmin`.
5. Switch to `default-safe.ini` or `best-feel-gt630m.ini`.
6. Set `frame_pacing_target` to **0.85 × Wmin**, rounded to a sensible value (60 / 75 / 90 / 120).

Examples:
- Worst-case 110 FPS on hard levels → set `frame_pacing_target=90`.
- Worst-case 65 FPS → set `frame_pacing_target=60`.
- Worst-case 200+ FPS (modern hardware) → set `frame_pacing_target` to your monitor refresh (60 / 144 / 165 / 240).

**Why below worst-case?** The pacer smooths drops only if there's GPU headroom. If `target` > your worst-case, the pacer is useless and frames just drop visibly during effects. Read the bilingual comment around `frame_pacing_target` in any of the example INIs for the full math.
