# Installing ReviANGLE

## TL;DR

1. **Backup** your `Geometry Dash` folder (or at minimum the existing `opengl32.dll` if any).
2. Download the latest [Release](https://github.com/nazarhktwitch/ReviANGLE/releases) ZIP - choose **DirectX 11** (most compatible) or **Vulkan** (modern GPUs).
3. Unzip into your GD install folder (where `GeometryDash.exe` lives).
4. Run `gd-angle-editor.exe` to tune (optional), or just launch GD.

---

## Step-by-Step Installation

### 1. Find your Geometry Dash folder

**Steam** (recommended):
```text
C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\
```

> *Path may differ based on where is your steam game install folder.*

**Standalone** (if installed elsewhere):
Look for the folder containing `GeometryDash.exe`.

---

### 2. Download the release

Go to https://github.com/nazarhktwitch/ReviANGLE/releases and download one of:
- `ReviANGLE-vX.Y.Z-DX11.zip` - Default, most compatible
- `ReviANGLE-vX.Y.Z-Vulkan.zip` - Modern GPUs (RTX, Radeon RX), best Vulkan performance

**Can't decide?** Start with DirectX 11 - it's stable on all hardware.

---

### 3. Unzip into Geometry Dash

**DirectX 11 build** contains:
```text
ReviANGLE-v1.1.0-DX11.zip
├── opengl32.dll              ← core proxy mod
├── libEGL.dll                ← ANGLE DLL
├── libGLESv2.dll             ← ANGLE DLL
├── d3dcompiler_47.dll        ← DirectX 11 compiler
├── angle_config.ini          ← config (editable)
├── gd-angle-editor.exe       ← configurator
├── ReviANGLE-Uninstall.exe   ← uninstaller
├── README.md
└── LICENSE
```

**Vulkan build** contains:
```text
ReviANGLE-v1.1.0-Vulkan.zip
├── opengl32.dll              ← core proxy mod
├── libEGL.dll                ← ANGLE DLL
├── libGLESv2.dll             ← ANGLE DLL
├── vulkan-1.dll              ← Vulkan runtime
├── angle_config.ini          ← config (editable)
├── gd-angle-editor.exe       ← configurator
├── ReviANGLE-Uninstall.exe   ← uninstaller
├── README.md
└── LICENSE
```

Extract all files directly into the GD folder so they end up next to `GeometryDash.exe`.

---

### 4. (Optional) Configure

Run `gd-angle-editor.exe`. The GUI shows every option with descriptions, current values, and impact estimates. Save your config when done.

Or edit `angle_config.ini` in any text editor.

---

### Linux & Steam Deck (Proton / Wine)

ReviANGLE is compatible with Linux & Steam Deck via Steam Play (Proton) or Wine:
1. Extract the release ZIP into your Geometry Dash folder next to `GeometryDash.exe`.
2. Right-click **Geometry Dash ➔ Properties ➔ Launch Options** in Steam.
3. Add the launch command:
   ```bash
   WINEDLLOVERRIDES="opengl32=n,b" %command%
   ```

---

### 5. Launch GD

Launch Geometry Dash. If everything works, you'll see:
- A new log file `angle_log.txt` next to `GeometryDash.exe`. **(ONLY IF THE DEBUG IS SET TO TRUE IN CONFIG)**
- Smoother gameplay and improved frame pacing.

---

## Uninstalling

### Using the Uninstaller (Recommended)

Run `ReviANGLE-Uninstall.exe` from your Geometry Dash directory:
1. It will auto-detect your GD installation.
2. Confirm uninstallation.
3. The tool removes all ReviANGLE files and restores your original `opengl32.dll` if a backup exists.

---

## Troubleshooting

### GD shows "WGL: Failed to create dummy context" or crash on launch

If you see a popup `GLFWError #65544: WGL: Failed to create dummy context`:
- Make sure you extracted all DLL files from the release ZIP (`libEGL.dll`, `libGLESv2.dll`, `d3dcompiler_47.dll` or `vulkan-1.dll`).
- If using Vulkan backend, verify your GPU drivers support Vulkan. Switch to the **DX11 release** if your GPU or driver doesn't support Vulkan.

### MegaHack overlay / mods

ReviANGLE is fully compatible with **MegaHack** and other GD mods. Legacy OpenGL 1.1 state exports (`glPushAttrib`, `glPopAttrib`, `glCopyTexImage2D`, `glCopyTexSubImage2D`) are implemented to preserve mod rendering overlays.
