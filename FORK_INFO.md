# About This ReviANGLE Fork

This is an **active fork** of the original [ReviANGLE](https://github.com/Reviusion/ReviANGLE) project by Reviusion, maintained to add and support **simultaneous DirectX 11 and Vulkan backends**.

## What Changed

### Core Additions

**1. Vulkan Backend Support**
- Full Vulkan rendering pipeline implemented alongside DirectX 11
- ANGLE DLLs (`libEGL.dll`, `libGLESv2.dll`) compiled separately from ANGLE source to include Vulkan support
- Separate build targets for DX11-only and Vulkan-only configurations
- Zero performance penalty - choose the backend at build time

**2. Modern Uninstaller**
- New `ReviANGLE-Uninstall.exe` tool with GUI
- Auto-detects Geometry Dash installation (Steam, Epic Games, or manual)
- Safely removes all mod files and caches
- Restores original `opengl32.dll.backup` if present
- Dual-backend aware (removes both DX11 and Vulkan DLLs as needed)
- Original uninstaller lacked source code and Vulkan support

**3. Updated Documentation**
- Clarified DirectX 11 vs Vulkan backend selection for end users
- Installation instructions for both backends
- Troubleshooting specific to each backend
- Build instructions for compiling both variants

**4. Configuration Updates**
- `angle_config.ini` now documents both backend options clearly
- GUI configurator (`gd-angle-editor.exe`) supports backend selection

## Upstream Synchronization

The original ReviANGLE repository remains **actively monitored**. As updates are released upstream, they will be:

1. **Analyzed** for compatibility with Vulkan implementation
2. **Merged** into this fork with proper attribution
3. **Tested** on both backends before release
4. **Released** as new versions maintaining fork features

## Building This Fork

### Prerequisites
- Visual Studio 2022 (C++ workload)
- CMake 3.20+
- Windows 10+

### Compile DirectX 11 build
```powershell
git clone https://github.com/nazarhktwitch/ReviANGLE.git
cd ReviANGLE
cmake -B build -A x64 -DREVIEWANGLE_BACKEND_D3D11=ON -DREVIEWANGLE_BACKEND_VULKAN=OFF
cmake --build build --config Release
```

### Compile Vulkan build
```powershell
cmake -B build-vulkan -A x64 -DREVIEWANGLE_BACKEND_D3D11=OFF -DREVIEWANGLE_BACKEND_VULKAN=ON
cmake --build build-vulkan --config Release
```

### Build outputs
```
build/Release/
├── opengl32.dll           (DirectX 11 proxy)
├── libEGL.dll             (ANGLE for DX11)
├── libGLESv2.dll          (ANGLE for DX11)
├── d3dcompiler_47.dll     (DirectX compiler)
├── gd-angle-editor.exe    (GUI configurator)
└── ReviANGLE-Uninstall.exe (uninstaller)

build-vulkan/Release/
├── opengl32.dll           (Vulkan proxy)
├── libEGL.dll             (ANGLE for Vulkan)
├── libGLESv2.dll          (ANGLE for Vulkan)
├── vulkan-1.dll           (Vulkan runtime)
├── gd-angle-editor.exe
└── ReviANGLE-Uninstall.exe
```

## Release Naming Convention

- **DX11 builds**: `ReviANGLE-vX.Y.Z-DX11-win64.zip`
- **Vulkan builds**: `ReviANGLE-vX.Y.Z-Vulkan-win64.zip`

Each contains the appropriate DLLs for that backend.

## Testing & Compatibility

Tested on:
- Intel integrated graphics (HD 630, UHD 730) with DirectX 11
- NVIDIA GeForce GTX 1060+ with both backends
- NVIDIA GeForce RTX 30-series with Vulkan
- AMD Radeon RX 6000-series with Vulkan

## Known Limitations & Considerations

1. **Cannot run both backends simultaneously** - CMake enforces exactly one per build
2. **ANGLE source code** - Not included in this repo; DLLs are pre-compiled binaries
3. **Original author attribution** - Full credit to Reviusion for the base project
4. **MIT License** - This fork maintains the same license as the original

## Contributing

Improvements to this fork are welcome:
- Bug fixes for either backend
- Documentation improvements
- Uninstaller enhancements
- Performance optimizations
- Compatibility reports

## License

MIT - see [`LICENSE`](LICENSE)

ANGLE library binaries are licensed under [BSD 3-Clause](https://chromium.googlesource.com/angle/angle/+/refs/heads/main/LICENSE).

## Credits

- **Original ReviANGLE**: Reviusion ([@Reviusion](https://github.com/Reviusion))
- **Vulkan backend & fork maintenance**: NazarHK ([@nazarhktwitch](https://github.com/nazarhktwitch))
- **ANGLE**: Google Chromium team
- **Dear ImGui**: Configurator GUI framework

---

**Note**: This is a community-maintained fork. For questions about the original project, refer to the [upstream repository](https://github.com/Reviusion/ReviANGLE).
