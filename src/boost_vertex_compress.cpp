// Boost: vertex compression
// Converts float32 vertex positions to int16 (short) format, halving bandwidth.
// DISABLED BY DEFAULT - precision loss may cause visual artifacts.
// Enable only if GPU is severely bandwidth-bottlenecked (like GT 630M on DDR3).

#include "angle_loader.hpp"
#include "config.hpp"
#include <windows.h>


namespace boost_vertex_compress {

void apply() {
  if (!Config::get().vertex_compress)
    return;

  // This requires intercepting glVertexAttribPointer and converting the
  // vertex buffer data in-place. The conversion must happen between
  // glBufferData and the draw call. Due to the high risk of visual
  // corruption (cocos2d expects float coords), this is left as a
  // placeholder. A real implementation would:
  //
  // 1. Hook glVertexAttribPointer for position attribs (index 0)
  // 2. When type == GL_FLOAT && size == 2:
  //    a. Map the VBO
  //    b. Convert float pairs to int16 pairs (scale by viewport size)
  //    c. Call real glVertexAttribPointer with GL_SHORT + normalized
  //
  // The scaling factor is tricky because cocos2d uses points, not pixels.

  angle::log("vertex_compress: enabled (placeholder - full impl pending)");
}
} // namespace boost_vertex_compress
