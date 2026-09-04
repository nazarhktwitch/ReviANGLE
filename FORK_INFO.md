# About This ReviANGLE Fork

This is an **active fork** of the original [ReviANGLE](https://github.com/Reviusion/ReviANGLE) project by Reviusion, maintained to support **simultaneous DirectX 11 and Vulkan backends** (also with CI/CD builds now!).

## What Changed

### Core Additions

**1. Vulkan Backend Support & MegaHack Compatibility**
- Full Vulkan rendering pipeline implemented alongside DirectX 11.
- Restored legacy OpenGL 1.1 exports and state tracking proxy (`glPushAttrib` / `glPopAttrib` attribute stack) to fix MegaHack overlay drawing & crashes.
- Statically separate build targets for DX11 (`D3D11`) and Vulkan configurations.

**2. Automated CI/CD Release Pipeline**
- Automated GitHub Actions workflow (`.github/workflows/release.yml`) builds `opengl32.dll`, `gd-angle-editor.exe`, and `ReviANGLE-Uninstall.exe` from source on version tags (`v*`).
- Dependency staging via `deps/dx11` and `deps/vulkan`.
- Prebuilt third-party ANGLE DLLs are kept in `deps/` while core project files are compiled live in CI for transparency.

**3. Automated Local Packaging Script**
- A PowerShell packaging script (`build_release.ps1`) to compile both DX11 and Vulkan builds locally and create release ZIP archives in one command.

**4. Uninstaller**
- `ReviANGLE-Uninstall.exe` tool with GUI.
- Auto-detects Geometry Dash installation.
- Safely removes all mod files and caches, restoring backup DLLs if present.

---

## Building This Fork

### Prerequisites
- Visual Studio 2022 (C++ workload)
- CMake 3.20+
- Windows 10/11

### Automated Local Build & Release Packaging

```powershell
git clone https://github.com/nazarhktwitch/ReviANGLE.git
cd ReviANGLE
.\build_release.ps1 -Version "v1.2.3" # Change version if needed!
```

### Manual CMake Commands

#### Compile DirectX 11 build
```powershell
cmake -B build_dx11 -A x64 -DREVIANGLE_BACKEND_D3D11=ON -DREVIANGLE_BACKEND_VULKAN=OFF
cmake --build build_dx11 --config Release
```

#### Compile Vulkan build
```powershell
cmake -B build_vulkan -A x64 -DREVIANGLE_BACKEND_D3D11=OFF -DREVIANGLE_BACKEND_VULKAN=ON
cmake --build build_vulkan --config Release
```

### Build Output Locations

```text
build_dx11/ (or build_vulkan/)
├── dll/Release/
│   ├── opengl32.dll              ← core proxy DLL
│   ├── gd-angle-editor.exe       ← configurator
└── bin/Release/
    └── ReviANGLE-Uninstall.exe   ← uninstaller
```

---

## Release Naming Convention

- **DX11 builds**: `ReviANGLE-vX.Y.Z-DX11.zip`
- **Vulkan builds**: `ReviANGLE-vX.Y.Z-Vulkan.zip`
