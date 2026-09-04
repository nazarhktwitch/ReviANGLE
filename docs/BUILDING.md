# Building ReviANGLE from source

## Prerequisites

| Tool | Minimum version | Notes |
|------|-----------------|-------|
| **Windows** | 11/10 (any build) | Windows 7/8 untested; some Win10-specific APIs degrade gracefully |
| **Visual Studio 2022** | 17.0 | Community edition is fine. Need **"Desktop development with C++"** workload. |
| **CMake** | 3.20+ | Bundled with VS or [download](https://cmake.org/download/) |
| **Git** | any | For cloning |

> **Note**: Geometry Dash 2.2+ is a 64-bit process, so the proxy DLL must be built for **x64**. The CMakeLists enforces this with a `CMAKE_GENERATOR_PLATFORM` check; pass `cmake -A x64` to be explicit.

## Step 1 - Clone

```powershell
git clone https://github.com/nazarhktwitch/ReviANGLE.git
cd ReviANGLE
```

## Step 2 - Get ANGLE prebuilts & Dependencies

The ANGLE backend libraries (`libEGL.dll`, `libGLESv2.dll`, `vulkan-1.dll`, `d3dcompiler_47.dll`) are stored under `deps/` in the repository for convenient release packaging:

- `deps/dx11/` - Dependencies for DirectX 11 backend
- `deps/vulkan/` - Dependencies for Vulkan backend

### Option A - Use repository prebuilts from `deps/` (easiest)

Simply leave the prebuilts in `deps/dx11/` or `deps/vulkan/`. The automated build script (`build_release.ps1`) and CI/CD workflow will automatically package them with your built binaries.

### ~~Option B - extract from Chromium / Edge~~ `[Deprecated]`

~~ANGLE is bundled with Chromium-based browsers. You can copy the three DLLs from:~~

```text
C:\Program Files (x86)\Microsoft\Edge\Application\<version>\
C:\Program Files\Google\Chrome\Application\<version>\
```

> **Note**: Vulkan is NOT SUPPORTED by these DLLs, so now to use Vulkan release you NEED to build it or extract pre-built from release or `deps/`.

### Option C - build ANGLE from source

See [ANGLE's official build instructions](https://chromium.googlesource.com/angle/angle/+/refs/heads/main/doc/DevSetup.md). This is a multi-hour process and **not recommended** unless you specifically need a custom ANGLE build.

---

## Step 3 - Configure & Build

### Automated Local Release Packaging (Recommended)

To build both **DirectX 11** and **Vulkan** releases and package them into ZIP archives automatically, run:

```powershell
.\build_release.ps1 -Version "vX.X.X" # Change "vX.X.X" to your desired version
```

This generates `ReviANGLE-vX.X.X-DX11.zip` and `ReviANGLE-vX.X.X-Vulkan.zip` in the project root.

---

### Manual CMake Build Commands

#### Build DirectX 11 Backend:

```powershell
cmake -B build_dx11 -A x64 -DREVIANGLE_BACKEND_D3D11=ON -DREVIANGLE_BACKEND_VULKAN=OFF
cmake --build build_dx11 --config Release
```

#### Build Vulkan Backend:

```powershell
cmake -B build_vulkan -A x64 -DREVIANGLE_BACKEND_D3D11=OFF -DREVIANGLE_BACKEND_VULKAN=ON
cmake --build build_vulkan --config Release
```

### Build Artifact Locations

After build, the output binaries will be generated in:

```text
build_dx11/ (or build_vulkan/)
├── dll/Release/
│   ├── opengl32.dll              ← core proxy DLL
│   ├── gd-angle-editor.exe       ← configurator app
│   └── ini_round_trip_test.exe   ← internal INI parser test
└── bin/Release/
    └── ReviANGLE-Uninstall.exe   ← uninstaller
```

---

## Step 4 - Test the build

```powershell
# Quick verify build artifacts exist & link is clean:
& build_dx11\dll\Release\ini_round_trip_test.exe examples_config\angle_config.default-safe.ini  # should exit 0
& build_dx11\dll\Release\gd-angle-editor.exe                                                  # should open Studio window
```

To test the actual mod in Geometry Dash, see [`INSTALLATION.md`](INSTALLATION.md).

---

## Build targets

| CMake target | Output | Description |
|--------------|--------|-------------|
| `opengl32` | `opengl32.dll` | Core proxy DLL |
| `gd_angle_editor` | `gd-angle-editor.exe` | GUI configurator for config options |
| `ReviANGLE-Uninstall` | `ReviANGLE-Uninstall.exe` | Uninstaller |
| `ini_round_trip_test` | `ini_round_trip_test.exe` | Validates INI parser preserves formatting |
| `ALL` (default) | all 4 | Build everything |

Build a single target with `cmake --build build_dx11 --config Release --target ReviANGLE-Uninstall`.

---

## Common build issues

### `error LNK2019: unresolved external symbol __imp_*`

Wrong architecture for the GD version you're targeting. GD 2.2+ is x64. Re-run cmake with the x64 architecture flag:
```powershell
Remove-Item -Recurse -Force build_dx11, build_vulkan
cmake -B build_dx11 -A x64 -DREVIANGLE_BACKEND_D3D11=ON -DREVIANGLE_BACKEND_VULKAN=OFF
cmake --build build_dx11 --config Release
```

### `cmake: command not found`

CMake isn't on PATH. Either install [CMake](https://cmake.org/download/) or use Developer PowerShell for VS 2022:
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -B build_dx11 -A x64
```

---

## Continuous Integration & Automated Releases

`.github/workflows/release.yml` automatically builds both **DirectX 11** and **Vulkan** release targets on every push to `main`/`dev` or when a version tag (e.g., `v1.2.0`) is pushed to GitHub.

CI artifacts and GitHub Release ZIP packages are attached automatically to the release page.
