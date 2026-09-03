# Installing ReviANGLE

## TL;DR

1. **Backup** your `Geometry Dash` folder (or at minimum the existing `opengl32.dll` if any).
2. Download the latest [Release](https://github.com/Reviusion/ReviANGLE/releases) ZIP - choose **DirectX 11** (most compatible) or **Vulkan** (modern GPUs).
3. Unzip into your GD install folder (where `GeometryDash.exe` lives).
4. Run `gd-angle-editor.exe` to tune (optional), or just launch GD.

## Step-by-step

### 1. Find your Geometry Dash folder

**Steam** (recommended):
```
C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\
```

**Standalone** (if downloaded directly):
Wherever you extracted it. Look for the folder containing `GeometryDash.exe`.

You should see `GeometryDash.exe` in this folder.

### 2. Download the release

Go to https://github.com/nazarhktwitch/ReviANGLE/releases and download one of:
- `ReviANGLE-vX.Y.Z-DX11-win64.zip` - Default, most compatible
- `ReviANGLE-vX.Y.Z-Vulkan-win64.zip` - Modern GPUs (RTX, Radeon RX), better performance

**Can't decide?** Start with DirectX 11 - it's stable on all hardware.

### 3. Unzip

**DirectX 11 build** contains:
```
ReviANGLE-v3.0.0-DX11-win64.zip
├── opengl32.dll              ← the proxy mod
├── libEGL.dll                ← ANGLE
├── libGLESv2.dll             ← ANGLE
├── d3dcompiler_47.dll        ← DirectX 11 compiler
├── angle_config.ini          ← config (editable)
├── gd-angle-editor.exe       ← GUI configurator
└── LICENSE
```

**Vulkan build** contains:
```
ReviANGLE-v3.0.0-Vulkan-win64.zip
├── opengl32.dll              ← the proxy mod
├── libEGL.dll                ← ANGLE
├── libGLESv2.dll             ← ANGLE
├── vulkan-1.dll              ← Vulkan runtime
├── angle_config.ini          ← config (editable)
├── gd-angle-editor.exe       ← GUI configurator
└── LICENSE
```

Extract all files directly into the GD folder so they end up next to `GeometryDash.exe`. Your folder should look like:

**If you installed DX11:**
```
Geometry Dash/
├── GeometryDash.exe
├── opengl32.dll              ← from ReviANGLE
├── libEGL.dll                ← from ReviANGLE
├── libGLESv2.dll             ← from ReviANGLE
├── d3dcompiler_47.dll        ← DirectX 11 (required for DX11 build)
├── angle_config.ini          ← from ReviANGLE
├── gd-angle-editor.exe       ← optional GUI
├── Resources/                ← original GD
└── ...                       ← other original GD files
```

**If you installed Vulkan:**
```
Geometry Dash/
├── GeometryDash.exe
├── opengl32.dll              ← from ReviANGLE
├── libEGL.dll                ← from ReviANGLE
├── libGLESv2.dll             ← from ReviANGLE
├── vulkan-1.dll              ← Vulkan (required for Vulkan build)
├── angle_config.ini          ← from ReviANGLE
├── gd-angle-editor.exe       ← optional GUI
├── Resources/                ← original GD
└── ...                       ← other original GD files
```

### 5. (Optional) Configure

Run `gd-angle-editor.exe`. The GUI shows every option with bilingual descriptions, current value, and impact estimates. Save your config when done.

Or edit `angle_config.ini` in any text editor - every option has full bilingual comments.

The shipped default is the **best-feel preset** for the developer's tested hardware (Intel i5-3230M + GT 630M, 90 Hz). If you have different hardware, the most important option to retune is `frame_pacing_target` - see the comment in the file.

### 6. Launch GD

If everything works, you'll see:
- A new file `angle_log.txt` next to `GeometryDash.exe` after first launch (mod's diagnostic log).
- Smoother gameplay, higher FPS, less input lag.

## Verifying it's working

Check `angle_log.txt`. Successful first lines with DirectX 11 look like:
```
[ReviANGLE] DllMain DLL_PROCESS_ATTACH
ReviANGLE attached - 84 boost modules, backend=d3d11
gpu_forcer: NvOptimusEnablement export installed
nvapi: using app profile for GeometryDash.exe
nvapi: PREFERRED_PSTATE=PreferMax = 0x00000000 applied
nvapi: DRS settings saved (5/5 applied)
workingset_lock: HARD floor=384 MB ceiling=1536 MB
frame_pacing: target dt = 8.333 ms (= 120 FPS), high-res waitable timer ENABLED
low_latency: MaxFrameLatency=1 (was 3)
gpu_thread_prio: GPU thread priority = +7 (max)
```

With Vulkan:
```
[ReviANGLE] DllMain DLL_PROCESS_ATTACH
ReviANGLE attached - 84 boost modules, backend=vulkan
...
```

If you see `ANGLE init failed`, `Cannot load vulkan-1.dll`, or similar backend errors, see [Troubleshooting](#troubleshooting) above.

## Uninstalling

### Using the Uninstaller (Recommended)

ReviANGLE includes an automated uninstaller with a GUI:

1. Download `ReviANGLE-Uninstall.exe` from the latest [Release](https://github.com/nazarhktwitch/ReviANGLE/releases)
2. Run it (it will auto-detect your Geometry Dash folder)
3. Or, place it in your Geometry Dash folder and run it from there
4. Confirm the uninstallation
5. The tool will remove all ReviANGLE files and restore your original `opengl32.dll` if a backup exists

### Manual Uninstall

If you prefer to remove files manually, delete these:
- `opengl32.dll` (the main proxy DLL)
- `libEGL.dll` (ANGLE core library)
- `libGLESv2.dll` (ANGLE OpenGL ES library)
- `d3dcompiler_47.dll` (DirectX 11 shader compiler, only if you installed the DirectX 11 build)
- `vulkan-1.dll` (Vulkan runtime, only if you installed the Vulkan build)
- `angle_config.ini` (configuration file)
- `gd-angle-editor.exe` (GUI configurator, optional)
- `angle_log.txt` (generated diagnostic log)
- `shader_cache/` folder (if it exists, generated at runtime)
- `plist_cache/` folder (if it exists, generated at runtime)

If you renamed an original `opengl32.dll.backup`, rename it back.

## Troubleshooting

### GD won't start at all

**Most common cause**: missing ANGLE DLLs. Verify these are next to `GeometryDash.exe`:
- `libEGL.dll`
- `libGLESv2.dll`
- If using DirectX 11 build: `d3dcompiler_47.dll`
- If using Vulkan build: `vulkan-1.dll`

If `angle_log.txt` doesn't appear, the mod's `DllMain` never ran - usually means the DLL is corrupted or for the wrong architecture (64-bit only for GD 2.2+). Re-download the correct build.

### GD starts but black screen / no rendering

Open `angle_config.ini` and check the `backend=` setting:

```ini
backend=d3d11    # Try this first (default, most compatible)
backend=vulkan   # Only if you have vulkan-1.dll and modern GPU
backend=d3d9     # Fallback for very old systems
debug=true       # Enables verbose logging
```

Then check `angle_log.txt` for backend selection and any errors.

**Backend recommendation:**
- **DirectX 11** (`d3d11`): Most widely compatible, especially on older laptops. Start here.
- **Vulkan** (`vulkan`): Better performance on modern GPUs (GTX 1000+, RTX, Radeon RX). Requires `vulkan-1.dll` in the folder and recent driver updates.
- **DirectX 9** (`d3d9`): Legacy fallback for very old systems (pre-2010).

### Lower FPS than vanilla GD

This usually means `frame_pacing_target` is too high for your GPU.

- If your **peak uncapped FPS** during effects is, say, 110 → set `frame_pacing_target=90` (with headroom).
- If you don't know your peak FPS: set `frame_pacing=false` once, run a hard level, watch FPS counter, then re-enable pacing with target ~ 80 % of your worst-case FPS.

See the long bilingual comment around `frame_pacing_target` in `angle_config.ini` - it has a full explanation with budget math.

### Crashes on launch with NVAPI errors in log

Set `nvapi_profile=false` in `angle_config.ini`. NVAPI tries to write to your Nvidia profile DB; some setups (e.g. corporate-managed PCs) deny this.

### Crashes on launch with "DLL search path" errors

Some antivirus / Windows Defender heuristics flag unsigned DLL injection. Add the GD folder to AV exceptions.

### "Failed to write to memory" crash inside `d3d11.dll`

This was a known old bug from `low_latency` calling the wrong vtable slot. Make sure you're running the latest release (check `angle_log.txt` first line).

### Online features broken

The default config sets `online_block_gameplay=true` (blocks network calls during levels for stability). To restore online features, set it to `false`.

### Crashes only on AMD / Intel GPUs

Disable the Nvidia-specific tweaks:
```ini
nvapi_profile=false
gpu_forcer=false
```

### Other mods stop working

ReviANGLE replaces `opengl32.dll`, which conflicts with any other mod that also replaces it. Known coexisting mods: **Eclipse Menu**. Known untested: **Geode**, **MegaHack v7**, **Mega Hack Pro**.

## Getting help

If none of the above fixes your issue:
1. Set `debug=true` in `angle_config.ini`.
2. Reproduce the issue.
3. Open a [GitHub Issue](https://github.com/Reviusion/ReviANGLE/issues/new/choose), attach `angle_log.txt`, and describe what happened.
