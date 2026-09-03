// Boost: SSE Flush-To-Zero + Denormals-Are-Zero
// Denormalized floating point numbers (very small, near zero) trigger a
// microcode assist on x86 CPUs, taking ~100 cycles instead of ~5.
// GD's particle systems and physics produce denormals frequently.
// FTZ+DAZ modes flush these to zero, keeping the FPU in fast-path.

#include "angle_loader.hpp"
#include "config.hpp"
#include <pmmintrin.h> // _MM_SET_DENORMALS_ZERO_MODE
#include <windows.h>
#include <xmmintrin.h> // _MM_SET_FLUSH_ZERO_MODE


namespace boost_ftz_daz {
void apply() {
  if (!Config::get().ftz_daz)
    return;

  // Set FTZ (Flush-To-Zero) - output denormals become zero
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);

  // Set DAZ (Denormals-Are-Zero) - input denormals treated as zero
  _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

  angle::log("ftz_daz: SSE FTZ+DAZ enabled - denormals flushed to zero");
}
} // namespace boost_ftz_daz
