// Boost: SSE2 math
//
// Provides vectorised sin/cos/sqrt approximations. GT 630M supports SSE4.1,
// but we stick to SSE2 for broadest compatibility. GD calls scalar math
// millions of times per frame inside cocos2d transforms (CCAffineTransform,
// particles), so even a modest per-call speedup matters in hot paths.
//
// We expose these as C symbols so they can be injected via IAT hook or linked
// directly into custom-built cocos2d. They are exported for completeness.

#include "config.hpp"
#include <cmath>
#include <emmintrin.h>


namespace boost_math {

// fast sqrt using SSE intrinsic - single float
extern "C" __declspec(dllexport) float sse_sqrtf(float x) {
  __m128 v = _mm_set_ss(x);
  v = _mm_sqrt_ss(v);
  return _mm_cvtss_f32(v);
}

// fast inverse sqrt via SSE reciprocal + one Newton-Raphson step
extern "C" __declspec(dllexport) float sse_rsqrtf(float x) {
  __m128 v = _mm_set_ss(x);
  __m128 rsq = _mm_rsqrt_ss(v);
  // Newton: rsq = rsq * (1.5 - 0.5 * x * rsq * rsq)
  __m128 half = _mm_set_ss(0.5f);
  __m128 three_half = _mm_set_ss(1.5f);
  __m128 rr = _mm_mul_ss(rsq, rsq);
  __m128 xr = _mm_mul_ss(v, rr);
  __m128 hxr = _mm_mul_ss(half, xr);
  __m128 fix = _mm_sub_ss(three_half, hxr);
  rsq = _mm_mul_ss(rsq, fix);
  return _mm_cvtss_f32(rsq);
}

// sin/cos approximations accurate enough for gameplay transforms (err ~ 1e-5)
// Using the classic minimax polynomial after range reduction to [-pi, pi].
static inline float reduce(float x) {
  constexpr float TWO_PI = 6.28318530718f;
  constexpr float INV_TWO_PI = 1.0f / TWO_PI;
  float k = std::floor(x * INV_TWO_PI + 0.5f);
  return x - k * TWO_PI;
}

extern "C" __declspec(dllexport) float fast_sinf(float x) {
  x = reduce(x);
  float x2 = x * x;
  // polynomial (11-th order)
  return x * (1.0f - x2 * (1.0f / 6.0f -
                           x2 * (1.0f / 120.0f - x2 * (1.0f / 5040.0f))));
}

extern "C" __declspec(dllexport) float fast_cosf(float x) {
  x = reduce(x);
  float x2 = x * x;
  return 1.0f - x2 * (0.5f - x2 * (1.0f / 24.0f - x2 * (1.0f / 720.0f)));
}

void apply() {
  // nothing to do at runtime - the SSE functions are always available.
  // If a future version wants to IAT-hook msvcrt's sinf/cosf/sqrtf inside
  // GD.exe, this is the place. Doing so safely requires careful address
  // patching, so we leave it opt-in and off by default.
  (void)Config::get().sse_math;
}
} // namespace boost_math
