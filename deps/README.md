# ReviANGLE Prebuilt Dependencies

This directory contains pre-compiled ANGLE libraries, Vulkan loader, D3DCompiler, and backend-specific default configuration files used during automated release builds.

## Directory Structure

- `deps/dx11/`:
  - `libEGL.dll` (ANGLE DirectX 11 backend)
  - `libGLESv2.dll` (ANGLE DirectX 11 backend)
  - `d3dcompiler_47.dll`
  - `angle_config.ini` (Default DirectX 11 configuration)

- `deps/vulkan/`:
  - `libEGL.dll` (ANGLE Vulkan backend)
  - `libGLESv2.dll` (ANGLE Vulkan backend)
  - `vulkan-1.dll` (Vulkan loader runtime)
  - `d3dcompiler_47.dll`
  - `angle_config.ini` (Default Vulkan configuration)

## Build Parity and Trust

The primary project binaries:
1. `opengl32.dll` (Core ReviANGLE proxy layer)
2. `ReviANGLE-Uninstall.exe` (GUI uninstaller)
3. `gd-angle-editor.exe` (GUI config editor)

Are **compiled from scratch from open source code** in this repository during automated GitHub Actions CI/CD builds.

If you prefer to build the ANGLE backend DLLs (`libEGL.dll`, `libGLESv2.dll`) yourself from Google ANGLE source code, you can build them independently and replace the files in `deps/dx11/` or `deps/vulkan/` before running the release packager script.
