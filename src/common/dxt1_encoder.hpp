#pragma once
#include <cstddef>
#include <cstdint>


// Minimal DXT1 (BC1) block encoder. Converts 4x4 RGBA8 blocks to 8-byte DXT1.
// Optimised for speed over quality - good enough for game sprites.

namespace dxt1 {

// Compress `width*height` RGBA8 image to DXT1.
// `dst` must be at least (width/4)*(height/4)*8 bytes.
// Width and height must be multiples of 4.
// Returns compressed size in bytes, or 0 on error.
size_t compress(const uint8_t *rgba, int width, int height, uint8_t *dst);

// Compress a single 4x4 block (64 bytes RGBA -> 8 bytes DXT1).
void compressBlock(const uint8_t block[64], uint8_t out[8]);

} // namespace dxt1
