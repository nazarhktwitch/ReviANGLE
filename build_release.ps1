# Local Release Packaging Script for ReviANGLE
param (
    [string]$Version = "v1.2.0"
)

$ErrorActionPreference = "Stop"

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " Building ReviANGLE Release $Version" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# Clean stale build caches if present
if (Test-Path "build_dx11") { Remove-Item "build_dx11" -Recurse -Force -ErrorAction SilentlyContinue }
if (Test-Path "build_vulkan") { Remove-Item "build_vulkan" -Recurse -Force -ErrorAction SilentlyContinue }

# 1. Build DX11
Write-Host "`n[1/4] Compiling DirectX 11 backend..." -ForegroundColor Yellow
cmake -B build_dx11 -A x64 -DCMAKE_BUILD_TYPE=Release -DREVIANGLE_BACKEND_D3D11=ON -DREVIANGLE_BACKEND_VULKAN=OFF
cmake --build build_dx11 --config Release --parallel

# 2. Build Vulkan
Write-Host "`n[2/4] Compiling Vulkan backend..." -ForegroundColor Yellow
cmake -B build_vulkan -A x64 -DCMAKE_BUILD_TYPE=Release -DREVIANGLE_BACKEND_D3D11=OFF -DREVIANGLE_BACKEND_VULKAN=ON
cmake --build build_vulkan --config Release --parallel

# 3. Prepare Staging Directories
Write-Host "`n[3/4] Staging artifacts..." -ForegroundColor Yellow
$outDir = "dist"
if (Test-Path $outDir) { Remove-Item $outDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path "$outDir/dx11" | Out-Null
New-Item -ItemType Directory -Force -Path "$outDir/vulkan" | Out-Null

# Copy DX11 compiled binaries
(Get-ChildItem -Path build_dx11 -Recurse -Filter "opengl32.dll" | Select-Object -First 1).FullName | Copy-Item -Destination "$outDir/dx11/"
(Get-ChildItem -Path build_dx11 -Recurse -Filter "gd-angle-editor.exe" | Select-Object -First 1).FullName | Copy-Item -Destination "$outDir/dx11/"
(Get-ChildItem -Path build_dx11 -Recurse -Filter "ReviANGLE-Uninstall.exe" | Select-Object -First 1).FullName | Copy-Item -Destination "$outDir/dx11/"

# Copy DX11 dependencies
Copy-Item deps/dx11/* "$outDir/dx11/" -Force
Copy-Item LICENSE "$outDir/dx11/"
Copy-Item README.md "$outDir/dx11/"

# Copy Vulkan compiled binaries
(Get-ChildItem -Path build_vulkan -Recurse -Filter "opengl32.dll" | Select-Object -First 1).FullName | Copy-Item -Destination "$outDir/vulkan/"
(Get-ChildItem -Path build_vulkan -Recurse -Filter "gd-angle-editor.exe" | Select-Object -First 1).FullName | Copy-Item -Destination "$outDir/vulkan/"
(Get-ChildItem -Path build_vulkan -Recurse -Filter "ReviANGLE-Uninstall.exe" | Select-Object -First 1).FullName | Copy-Item -Destination "$outDir/vulkan/"

# Copy Vulkan dependencies
Copy-Item deps/vulkan/* "$outDir/vulkan/" -Force
Copy-Item LICENSE "$outDir/vulkan/"
Copy-Item README.md "$outDir/vulkan/"

# 4. Create ZIP archives
Write-Host "`n[4/4] Creating ZIP archives..." -ForegroundColor Yellow
$dx11Zip = "ReviANGLE-$Version-DX11.zip"
$vulkanZip = "ReviANGLE-$Version-Vulkan.zip"

if (Test-Path $dx11Zip) { Remove-Item $dx11Zip -Force }
if (Test-Path $vulkanZip) { Remove-Item $vulkanZip -Force }

Compress-Archive -Path "$outDir/dx11/*" -DestinationPath $dx11Zip -Force
Compress-Archive -Path "$outDir/vulkan/*" -DestinationPath $vulkanZip -Force

Write-Host "`n==================================================" -ForegroundColor Green
Write-Host " RELEASE PACKAGING COMPLETE!" -ForegroundColor Green
Write-Host " Archives generated:" -ForegroundColor Green
Write-Host "   - $dx11Zip ($( (Get-Item $dx11Zip).Length ) bytes)" -ForegroundColor Green
Write-Host "   - $vulkanZip ($( (Get-Item $vulkanZip).Length ) bytes)" -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Green
