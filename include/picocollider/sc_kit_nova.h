// sc_kit_nova.h — nova vec math helpers lifted verbatim from
// rack-rp2350/src/dronecaster/drones/drone_common.h (GPL-3.0; nova
// vec_math.hpp). Lifted for the Benjolin port (2026-11-22): the SynthDef's
// `output.tanh` soft-clip runs through vec_tanh_float, and its law is subtle
// (the small path keeps the SIGNED arg). Only the tanh dependency closure is
// lifted — NovaRint, NovaLdexp, NovaExpTanh, NovaTanh; NovaLog/NovaExp/
// NovaPow stay in drone_common.h (YAGNI; the benjolin engine uses the
// per-sample double-pow midicps law instead of NovaMidi2Freq).
//
// Anchors: vec_math.hpp:483 (exp_tanh), :516 (tanh small path), vec/vec_sse
// round/truncate mapping; drone_common.h:222-286 (helpers), :335-369 (tanh).
#ifndef RACK_PICOCOLLIDER_SC_KIT_NOVA_H_
#define RACK_PICOCOLLIDER_SC_KIT_NOVA_H_
#include <cmath>
#include <cstdint>
#include <cstring>

namespace sc {

// _mm_round_ps(_MM_FROUND_TO_NEAREST_INT): nearest, ties to even.
inline float NovaRint(float x) { return std::rintf(x); }

// nova ldexp_float: pure bit manipulation (slli(n, 23) + exponent bits).
inline float NovaLdexp(float p, int32_t n) {
  uint32_t bits;
  std::memcpy(&bits, &p, 4);
  const int32_t new_exp =
      (n << 23) + static_cast<int32_t>(bits & 0x7F800000u);
  const uint32_t out = (bits & 0x807FFFFFu) | static_cast<uint32_t>(new_exp);
  float r;
  std::memcpy(&r, &out, 4);
  return r;
}

// nova vec_exp_tanh_float (the boundary-check-free exp used by tanh).
inline float NovaExpTanh(float arg) {
  float x = arg;
  const float z = NovaRint(1.44269504088896341f * x);
  const int32_t n = static_cast<int32_t>(z);
  x -= z * 0.693359375f;
  x -= z * -2.12194440e-4f;
  const float p = 1.0f +
      x * (1.00000035762786865234375f +
           x * (0.4999996721744537353515625f +
                x * (0.16665561497211456298828125f +
                     x * (4.167006909847259521484375e-2f +
                          x * (8.420792408287525177001953125e-3f +
                               x * 1.386119984090328216552734375e-3f)))));
  return NovaLdexp(p, n);
}

// nova vec_tanh_float (cephes-style SSE polynomial; branches are lane masks).
// LAW (vec_math.hpp:483-524): the SMALL path (|x| < 0.625) evaluates the odd
// polynomial against the SIGNED arg (`* arg_sqr * arg + arg`) — dropping the
// sign there silently mirrors the negative half into the positive (found the
// hard way via Mt. Lion, whose comb output sits at |x| ≈ 0.625).
inline float NovaTanh(float x) {
  uint32_t xb;
  std::memcpy(&xb, &x, 4);
  const uint32_t sign = xb & 0x80000000u;
  const uint32_t absb = xb & 0x7FFFFFFFu;
  float abs_arg;
  std::memcpy(&abs_arg, &absb, 4);
  // medium values: 1 - 2/(exp(2|x|) + 1)
  const float medium = 1.0f - 2.0f / (NovaExpTanh(abs_arg + abs_arg) + 1.0f);
  // large values (|x| > 22): +-1
  float result_lm_abs = (abs_arg > 22.0f) ? 1.0f : medium;
  uint32_t lmb;
  std::memcpy(&lmb, &result_lm_abs, 4);
  lmb |= sign;
  std::memcpy(&result_lm_abs, &lmb, 4);
  // small values (|x| < 0.625): odd polynomial in the SIGNED x (nova
  // vec_math.hpp:516 — `* arg_sqr * arg + arg` — the sign must survive;
  // Mt. Lion's comb values sit right at this boundary)
  const float f1 = -5.70498872745e-3f;
  const float f2 = 2.06390887954e-2f;
  const float f3 = -5.37397155531e-2f;
  const float f4 = 1.33314422036e-1f;
  const float f5 = -3.33332819422e-1f;
  const float arg_sqr = abs_arg * abs_arg;
  const float result_small =
      ((((f1 * arg_sqr + f2) * arg_sqr + f3) * arg_sqr + f4) * arg_sqr + f5) *
          arg_sqr * x +
      x;
  return (abs_arg < 0.625f) ? result_small : result_lm_abs;
}

}  // namespace sc
#endif  // RACK_PICOCOLLIDER_SC_KIT_NOVA_H_
