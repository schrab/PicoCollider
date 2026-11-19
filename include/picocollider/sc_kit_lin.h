// sc_kit_lin.h — SC 3.14.1 linear-mapping kit: the LinExp core UGen
// (GPL-3.0, server/plugins/LFUGens.cpp) and the LinLin classlib mul-add
// expansion (GPL-3.0, SCClassLibrary/Common/Audio/Line.sc + Common/Math/).
// Neither UGen takes a clip argument: clip=\minmax is the LANGUAGE-side
// SimpleNumber default and never reaches the server graphs.
//
// LinExp (LFUGens.cpp, Version-3.14.1):
//   :99   struct LinExp : public Unit { float m_dstratio, m_rsrcrange,
//         m_rrminuslo, m_dstlo; } — ALL float members.
//   :2361 LinExp_Ctor: m_dstlo = dstlo; m_dstratio = dsthi / dstlo (float
//         division); m_rsrcrange = 1. / (srchi - srclo) (DOUBLE division,
//         rounded into the float member); m_rrminuslo = m_rsrcrange *
//         -srclo (float mul); then LinExp_next(unit, 1) primes one sample.
//   :2203 LinExp_next: out = dstlo * pow(dstratio, in * rsrcrange +
//         rrminuslo). The compiled x86-64 plugin calls powf@GLIBC (verified
//         in the .so disassembly) — the scalar float law.
//   Rate selection (LinExp_SetCalc :2341-2360): with all-scalar mapping
//   args and NOVA_SIMD, an AR unit takes LinExp_next_nova when
//   BUFLENGTH % (2*nova::vec<float>::size) == 0 (64 % 8 == 0 at the server's
//   64-sample block) — nova's VECTORIZED pow, which is NOT powf:
//   nova-simd detail::vec_math.hpp vec_pow = exp(arg2 * log(arg1)) with
//   vec_exp_float/vec_log_float (cephles-derived float lanes; `round` is the
//   magic-number trick because the Arch build is -march=x86-64, no SSE4.1
//   roundps in the .so). A KR unit's BUFLENGTH is 1 → the scalar
//   LinExp_next/powf path. Both laws are implemented here; the kr and ar
//   references differ by those ULPs, so they are NOT interchangeable.
//
// LinLin (Line.sc:29-40): classlib expansion, no core UGen.
//   *ar: MulAdd(in, scale, offset) — MulAddUGens.cpp next_ai: per sample
//        out = in * mul + add in plain float ops (no FMA at -march=x86-64).
//   *kr: (in * scale + offset) — two BinaryOpUGens, the same two float
//        roundings per sample. scale/offset are computed in sclang DOUBLES
//        (language) and rounded once into the float32 synth-def args, so
//        Init() replicates: double math from the promoted float args, one
//        float rounding each (verified against sclang for the canonical
//        graph: scale -0.47 -> 0xBEF0A3D7, offset 0.8 -> 0x3F4CCCCD).
//
// House conventions: Init() = the ctor coefficient setup (no prime — the
// ctor's one primed sample is either discarded by the ar block calc or is
// exactly the block-0 kr value); ctor_value(in_ctor) returns what K2A reads
// at ctor time for the given input prime (SinOsc.kr primes to 0).
#ifndef RACK_SRC_DRONECASTER_SC_KIT_LIN_H_
#define RACK_SRC_DRONECASTER_SC_KIT_LIN_H_
#include <cmath>
#include <cstdint>
#include <cstring>
#include "sc_defs.h"

namespace sc {

namespace lin_detail {

inline uint32_t BitsOf(float f) {
  uint32_t u;
  memcpy(&u, &f, sizeof(u));
  return u;
}
inline float FloatOf(uint32_t u) {
  float f;
  memcpy(&f, &u, sizeof(f));
  return f;
}

// nova detail::vec_round_float (vec_math.hpp): (|arg| + 0x1.0p23f) - 0x1.0p23f,
// sign reapplied — round-half-to-even in the float add. Used by the SSE2
// build (no __SSE4_1__ roundps at -march=x86-64).
inline float NovaRoundF(float arg) {
  const uint32_t sign = BitsOf(arg) & 0x80000000u;
  const float abs_arg = FloatOf(BitsOf(arg) ^ sign);
  const float two23 = 0x1.0p23f;
  const float rounded = (abs_arg + two23) - two23;
  return FloatOf(BitsOf(rounded) ^ sign);
}

// nova detail::vec_exp_float — cephes-derived float lane law.
inline float NovaExpF(float arg) {
  float x = arg;
  const float z = NovaRoundF(1.44269504088896341f * x);
  const int n = static_cast<int>(z);            // truncate_to_int (cvttps)
  x -= z * 0.693359375f;
  x -= z * -2.12194440e-4f;
  float p = 1.0f +
      x * (1.00000035762786865234375f +
      x * (0.4999996721744537353515625f +
      x * (0.16665561497211456298828125f +
      x * (4.167006909847259521484375e-2f +
      x * (8.420792408287525177001953125e-3f +
      x * 1.386119984090328216552734375e-3f)))));
  // ldexp_float(p, n): splice n into the exponent field of p.
  const uint32_t pb = BitsOf(p);
  const uint32_t expb = pb & 0x7F800000u;
  const uint32_t mant = pb & ~0x7F800000u;
  const int32_t ne = (n << 23) + static_cast<int32_t>(expb);
  float approx = FloatOf(mant | static_cast<uint32_t>(ne));
  // boundary selects (mask_gt / mask_lt) — inactive for the LinExp range,
  // kept for law fidelity.
  if (arg > 88.72283905206835f) approx = FloatOf(0x7F7FFFFFu);
  if (arg < -88.0f) approx = 0.0f;
  return approx;
}

// nova detail::vec_log_float — cephes-derived float lane law.
inline float NovaLogF(float x0) {
  const uint32_t xb = BitsOf(x0);
  int e = static_cast<int>((xb & 0x7F800000u) >> 23) - 126;   // frexp exponent
  float x = FloatOf((xb & ~0x7F800000u) | 0x3F000000u);       // | 0.5f bits
  const float sqrt_05 = 0.707106781186547524f;
  const bool small = x < sqrt_05;               // mask_lt
  if (small) e -= 1;                            // e + int_vec(mask)
  const float x_add = small ? x : 0.0f;         // x & mask
  x = x + (x_add - 1.0f);                       // x += x_add - gen_one()
  float y = (((((((( 7.0376836292E-2f * x
  - 1.1514610310E-1f) * x
  + 1.1676998740E-1f) * x
  - 1.2420140846E-1f) * x
  + 1.4249322787E-1f) * x
  - 1.6668057665E-1f) * x
  + 2.0000714765E-1f) * x
  - 2.4999993993E-1f) * x
  + 3.3333331174E-1f) * x * x*x;
  const float fe = static_cast<float>(e);       // convert_to_float
  y += fe * -2.12194440e-4f;
  y -= 0.5f * x * x;                            // (0.5 * x) * x
  const float z = x + y;
  return z + 0.693359375f * fe;
}

// nova detail::vec_pow: exp(arg2 * log(arg1)), arg1 == 0 -> 0 (select guard).
inline float NovaPowF(float arg1, float arg2) {
  if (arg1 == 0.0f) return 0.0f;
  return NovaExpF(arg2 * NovaLogF(arg1));
}

}  // namespace lin_detail

// LinExp — core UGen (LFUGens.cpp:99/2203/2361). Kr uses the scalar powf
// law (kr BUFLENGTH 1 → LinExp_next); ar uses nova's vectorized pow law
// (LinExp_next_nova, all-scalar args + 64-sample blocks).
struct LinExp {
  void Init(float srclo, float srchi, float dstlo, float dsthi) {
    dstlo_ = dstlo;                                            // :2363
    dstratio_ = dsthi / dstlo;                                 // :2364 float div
    rsrcrange_ = static_cast<float>(1.0 / (srchi - srclo));    // :2365 double div
    rrminuslo_ = rsrcrange_ * -srclo;                          // :2366 float mul
  }
  // LinExp_next scalar law (kr; powf@GLIBC in the compiled plugin).
  float Next(float in) const {
    return dstlo_ * std::pow(dstratio_, in * rsrcrange_ + rrminuslo_);
  }
  float ProcessKr(float in) const { return Next(in); }
  // LinExp_Ctor primes one sample (LinExp_next(unit, 1)); K2A's
  // ControlRateInput reads exactly this value at ctor time.
  float ctor_value(float in_ctor) const { return Next(in_ctor); }
  // LinExp_next_nova law (ar): dstlo * pow(dstratio, in * rsrcrange + rrminuslo)
  // with nova's vec pow = exp(y * log(a)) float lanes.
  void ProcessBlock(const float* in, float* out, int n) const {
    for (int i = 0; i < n; ++i) {
      out[i] = dstlo_ *
          lin_detail::NovaPowF(dstratio_, in[i] * rsrcrange_ + rrminuslo_);
    }
  }

 private:
  float dstlo_ = 0.0f, dstratio_ = 0.0f, rsrcrange_ = 0.0f, rrminuslo_ = 0.0f;
};

// LinLin — classlib mul-add expansion (Line.sc:29-40), no clip argument.
// ar = MulAdd(in, scale, offset); kr = the same two float roundings via
// BinaryOpUGens (in * scale) + offset.
struct LinLin {
  void Init(float srclo, float srchi, float dstlo, float dsthi) {
    // Language doubles, then one float32 rounding per synth-def arg.
    const double scale =
        (static_cast<double>(dsthi) - static_cast<double>(dstlo)) /
        (static_cast<double>(srchi) - static_cast<double>(srclo));
    const double offset = static_cast<double>(dstlo) -
        scale * static_cast<double>(srclo);
    scale_ = static_cast<float>(scale);
    offset_ = static_cast<float>(offset);
  }
  float Next(float in) const { return in * scale_ + offset_; }
  float ProcessKr(float in) const { return Next(in); }
  // The kr expansion primes its BinaryOp chain with the input's ctor value;
  // K2A ramps from that value.
  float ctor_value(float in_ctor) const { return Next(in_ctor); }
  void ProcessBlock(const float* in, float* out, int n) const {
    for (int i = 0; i < n; ++i) out[i] = in[i] * scale_ + offset_;
  }

 private:
  float scale_ = 0.0f, offset_ = 0.0f;
};

}  // namespace sc
#endif  // RACK_SRC_DRONECASTER_SC_KIT_LIN_H_
