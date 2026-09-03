# Building ReviANGLE from source

## Prerequisites

| Tool | Minimum version | Notes |
|------|-----------------|-------|
| **Windows** | 10 (any build) | Windows 7/8 untested; some Win10-specific APIs degrade gracefully |
| **Visual Studio 2022** | 17.0 | Community edition is fine. Need **"Desktop development with C++"** workload. |
| **CMake** | 3.20+ | Bundled with VS or [download](https://cmake.org/download/) |
| **Git** | any | For cloning |

> **Note**: Geometry Dash 2.2+ is a 64-bit process, so the proxy DLL must be built for **x64**. The CMakeLists enforces this with a `CMAKE_GENERATOR_PLATFORM` check; pass `cmake -A x64` to be explicit.

## Step 1 - Clone

```powershell
git clone https://github.com/Reviusion/ReviANGLE.git
cd ReviANGLE
```

## Step 2 - Get ANGLE prebuilts

The ANGLE library (`libEGL.dll`, `libGLESv2.dll`, `d3dcompiler_47.dll`) is **not** part of this repo - those binaries are large and have their own license. Three options:

### Option A - copy from a release ZIP (easiest)

Download the latest release from [Releases](https://github.com/Reviusion/ReviANGLE/releases), unzip, and copy:
```
libEGL.dll
libGLESv2.dll
d3dcompiler_47.dll
```
into a folder you'll later use for testing. (Build output doesn't depend on these - they're loaded at runtime.)

### Option B - extract from Chromium / Edge

ANGLE is bundled with Chromium-based browsers. You can copy the three DLLs from:
```
C:\Program Files (x86)\Microsoft\Edge\Application\<version>\
C:\Program Files\Google\Chrome\Application\<version>\
```

### Option C - build ANGLE from source

See [ANGLE's official build instructions](https://chromium.googlesource.com/angle/angle/+/refs/heads/main/doc/DevSetup.md). This is a multi-hour process and **not recommended** unless you specifically need a custom ANGLE build.

## Step 3 - Configure & build

```powershell
# from the repo root
cmake -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

After build, you'll have:
```
build/Release/
├── opengl32.dll              ← the proxy (drop this into GD folder)
├── opengl32.lib              ← intermediate
├── opengl32.exp              ← intermediate
├── gd-angle-editor.exe       ← GUI configurator
├── ReviANGLE-Uninstall.exe   ← uninstaller with modern GUI
└── ini_round_trip_test.exe   ← internal test (optional)
```

## Step 4 - Test the build

```powershell
# Quick verify build artifacts exist & link is clean:
& build\Release\ini_round_trip_test.exe   # should exit 0 with "ROUND-TRIP OK"
& build\Release\gd-angle-editor.exe       # should open ReviANGLE Studio window
```

To test the actual mod, see [`INSTALLATION.md`](INSTALLATION.md).

## Build targets

| CMake target | Output | Description |
|--------------|--------|-------------|
| `opengl32` | `opengl32.dll` | The actual proxy mod |
| `gd_angle_editor` | `gd-angle-editor.exe` | GUI configurator for config options |
| `ReviANGLE-Uninstall` | `ReviANGLE-Uninstall.exe` | Auto-detecting uninstaller with modern GUI |
| `ini_round_trip_test` | `ini_round_trip_test.exe` | Validates INI parser preserves formatting |
| `ALL` (default) | all 4 | Build everything |

Build a single target with `cmake --build build --config Release --target ReviANGLE-Uninstall`.

## Common build issues

### `error LNK2019: unresolved external symbol __imp_*`

Wrong architecture for the GD version you're targeting. GD 2.2+ is x64; older builds were x86. Re-run cmake with the correct flag:
```powershell
Remove-Item -Recurse -Force build
cmake -B build -A x64       # for GD 2.2+ (default)
# or:
cmake -B build -A Win32     # for GD 2.1 / older
cmake --build build --config Release
```

### `Cannot open include file: 'imgui.h'`

The configurator depends on Dear ImGui, which is fetched as part of the build. If CMake's `FetchContent` fails (firewall, etc.), check `build/_deps/imgui-src/` exists.

### `cmake: command not found`

CMake isn't on PATH. Either install [CMake](https://cmake.org/download/) and reboot, or use the version bundled with VS 2022:
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -B build -A x64
```

### Build succeeds but `opengl32.dll` is 0 KB

You probably ran cmake without specifying a generator, and it picked something incompatible. Force the VS generator:
```powershell
cmake -B build -A x64 -G "Visual Studio 17 2022"
```

### NVAPI link errors

The mod loads NVAPI dynamically at runtime via `LoadLibraryA("nvapi.dll")` - you should **not** be linking `nvapi.lib` at build time. If you see NVAPI link errors, check `boost_nvapi.cpp` is the only file referencing NVAPI symbols and uses dynamic loading.

## Continuous integration

`.github/workflows/build.yml` runs the build on every push. See the [Actions tab](https://github.com/Reviusion/ReviANGLE/actions) for the latest build status. CI artifacts are attached to each successful run - useful if you want a build without running the toolchain locally.
