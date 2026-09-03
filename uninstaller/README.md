# ReviANGLE Uninstaller

Uninstaller for ReviANGLE with support for both DirectX 11 and Vulkan backends.

## Features

- **Auto-detection**: Automatically finds Geometry Dash installation (Steam or manual selection)
- **Safe removal**: Cleanly removes all ReviANGLE files and folders
- **Dual-backend support**: Removes DLLs from both DirectX 11 and Vulkan builds
- **Detailed reporting**: Shows exactly which files were deleted and any issues encountered

## Building

This is built as part of the main ReviANGLE CMake project:

```powershell
cd ReviANGLE
cmake -B build -A x64
cmake --build build --config Release
```

The executable will be generated as:
- `build\Release\ReviANGLE-Uninstall.exe`

## Usage

Simply run `ReviANGLE-Uninstall.exe`:

1. The uninstaller will try to auto-detect your Geometry Dash installation from Steam
2. If not found, you'll be prompted to select `GeometryDash.exe` manually
3. Confirm the uninstallation
4. All ReviANGLE files will be removed
5. Review the summary of deleted files

## Files Removed

### DLLs
- `opengl32.dll` - the proxy
- `libEGL.dll` - ANGLE core library
- `libGLESv2.dll` - ANGLE OpenGL ES library  
- `d3dcompiler_47.dll` - DirectX 11 compiler (if present)
- `vulkan-1.dll` - Vulkan runtime (if present)

### Configuration & Tools
- `angle_config.ini` - configuration file
- `gd-angle-editor.exe` - GUI configurator
- `angle_log.txt` - diagnostic log

### Cache
- `shader_cache/` - cached shaders
- `plist_cache/` - cached property lists

## Build Requirements

- Visual Studio 2022 or later (MSVC toolset)
- CMake 3.20+
- Windows 10 or later

## License

MIT - same as ReviANGLE
