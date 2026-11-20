// sc_kit_osc.h — SC 3.14.1 wavetable + SinOsc + LFTri ports (GPL-3.0).
// Sources (Version-3.14.1): common/Samp.cpp (FillTables/SignalAsWavetable),
// server/plugins/OscUGens.cpp:1257-1351 (Osc_iai_perform, SinOsc_Ctor),
// server/plugins/LFUGens.cpp:654-677 (LFTri_next_a/next_k, LFTri_Ctor),
// include/plugin_interface/SC_SndBuf.h (PhaseFrac1/lookupi1).
//
// 3.14.1 deltas vs the 3.13.1 rev-2 plan (verified against the tag):
// - Osc_*_perform copies the double m_cpstoinc into a FLOAT local, so the
//   per-sample phase increment is computed as (int32)(float(cpstoinc)*freq).
// - LFTri_Ctor now UNDOES its priming call (m_phase = initPhase), so the
//   first rendered sample sits at phase 0 (z = 0) — 3.13.1 advanced one step.
//
// NO software denormal handling (FPSCR FZ/DN is global on both cores).
#ifndef RACK_SRC_DRONECASTER_SC_KIT_OSC_H_
#define RACK_SRC_DRONECASTER_SC_KIT_OSC_H_
#include <cmath>
#include <cstdint>
#include "sc_defs.h"

namespace sc {

constexpr int kSineSize = 8192;      // include/plugin_interface/SC_Complex.h
constexpr float kBadValue = 1e20f;   // include/plugin_interface/SC_Constants.h

// T-D2 device memory law: the two BLIT tables (gSine / gInvSine, 8,193 floats
// each) are LAZY. The pan family (Pan2/Balance2/SelectX) only indexes the
// first quarter of gSine, so that quarter is INLINE and always built; the
// full sine + cosecant (used by Pulse, the BLIT Saw, Rotate2, PanAz) are
// built on demand into an arena block via EnsurePulseTables
// (dronecaster_engine.h). Values are identical either way (same sin() law).
struct KitTables {
  float* sine_full;                  // SC gSine[0..8192] when built, else null
  float* cosecant;                   // SC gInvSine[0..8192] when built, else null
  float sine_wt[2 * kSineSize];      // SC gSineWavetable: pairs {2a-b, b-a}
  float sine_q[kSineSize / 4 + 2];   // SC gSine[0..2048] + 1 pad (8-align)
};
static_assert(sizeof(KitTables) ==
                  2 * sizeof(float*) + (2 * kSineSize + kSineSize / 4 + 2) * 4,
              "KitTables layout/padding changed");

// SC_SndBuf.h PhaseFrac
inline float PhaseFrac(uint32_t inPhase) {
  union { uint32_t itemp; float ftemp; } u;
  u.itemp = 0x3F800000u | (0x007FFF80u & (inPhase << 7));
  return u.ftemp - 1.0f;
}
inline float PhaseFrac1(uint32_t inPhase) {
  union { uint32_t itemp; float ftemp; } u;
  u.itemp = 0x3F800000u | (0x007FFF80u & (inPhase << 7));
  return u.ftemp;
}
inline float lininterp(float x, float a, float b) { return a + x * (b - a); }

// SC_SndBuf.h lookupi1 (table1 must be table0 + 1 float)
inline float lookupi1(const float* table0, const float* table1,
                      uint32_t pphase, int32_t lomask) {
  const float pfrac = PhaseFrac1(pphase);
  const uint32_t index = ((pphase >> 13) & static_cast<uint32_t>(lomask));
  const float val1 = *reinterpret_cast<const float*>(
      reinterpret_cast<const char*>(table0) + index);
  const float val2 = *reinterpret_cast<const float*>(
      reinterpret_cast<const char*>(table1) + index);
  return val1 + val2 * pfrac;
}

// The single sine-sample law (SC FillTables: float32 d = sin(phase), phase =
// i * 2pi / kSineSize, double math). Every table (wavetable pairs, the pan
// quarter, the full BLIT tables) derives from this one formula, so a lazily
// built table is bit-identical to the eagerly built one.
inline float SineTableSample(int i) {
  const double twopi = 6.283185307179586476925286766559;
  const double sineIndexToPhase = twopi / kSineSize;
  return static_cast<float>(std::sin(static_cast<double>(i) * sineIndexToPhase));
}

inline void InitKitTables(KitTables* t) {
  // SignalAsWavetable(gSine, gSineWavetable, kSineSize) — common/Samp.cpp.
  // The pairs come from consecutive sine samples; they are computed from the
  // sine law directly (the full gSine array may not exist yet — lazy BLIT
  // tables, T-D2 device memory law).
  float val1, val2;
  float* out = t->sine_wt - 1;
  for (int i = 0; i < kSineSize - 1; ++i) {
    val1 = SineTableSample(i);
    val2 = SineTableSample(i + 1);
    *++out = 2.0f * val1 - val2;
    *++out = val2 - val1;
  }
  val1 = SineTableSample(kSineSize - 1);
  val2 = SineTableSample(0);
  *++out = 2.0f * val1 - val2;
  *++out = val2 - val1;
  // The pan quarter (gSine[0..2048]) — Pan2/Balance2/SelectX index range.
  for (int i = 0; i <= kSineSize / 4; ++i) t->sine_q[i] = SineTableSample(i);
  t->sine_full = nullptr;   // BLIT tables are built on demand
  t->cosecant = nullptr;
}

// Builds the full gSine + gInvSine tables into `block` (2*(kSineSize+1)
// floats, caller-owned/arena) and points the KitTables at them.
// common/Samp.cpp FillTables: gSine[i] = (float)sin(phase); gInvSine[i] =
// (float)(1.0 / (double)d); then the kBadValue bands around the discontinuity.
inline void BuildPulseTables(KitTables* t, float* block) {
  float* full = block;
  float* inv = block + (kSineSize + 1);
  for (int i = 0; i <= kSineSize; ++i) {
    const float d = SineTableSample(i);
    full[i] = d;
    inv[i] = static_cast<float>(1.0 / static_cast<double>(d));
  }
  // kBadValue bands — common/Samp.cpp
  inv[0] = inv[kSineSize / 2] = inv[kSineSize] = kBadValue;
  const int sz = kSineSize;
  const int sz2 = sz >> 1;
  for (int i = 1; i <= 8; ++i) {
    inv[i] = inv[sz - i] = kBadValue;
    inv[sz2 - i] = inv[sz2 + i] = kBadValue;
  }
  t->sine_full = full;
  t->cosecant = inv;
}

// SinOsc (OscUGens.cpp:1325-1351 Ctor, 1302-1320 Osc_iai_perform).
// a-rate freq (K2A) or i-rate both reduce to the same loop with a
// block-constant freq. The ctor's priming call is UNDONE by
// `unit->m_phase = initPhase` — the first rendered sample is at phase 0.
// The loop reads at the current phase, then increments by
// (int32)(cpstoinc_f * freq) computed in FLOAT (the perform copies the
// double member into a float local).
struct SinOsc {
  void Init(const KitTables* t, double sr, float freq) {
    t_ = t;
    cpstoinc_ = static_cast<float>(static_cast<double>(kSineSize) * (1.0 / sr)
                                   * 65536.0);
    lomask_ = (kSineSize - 1) << 3;
    phase_ = 0;
    freq_ = freq;
  }
  void SetFreq(float f) { freq_ = f; }
  float Process() {
    const float z = lookupi1(t_->sine_wt, t_->sine_wt + 1, phase_, lomask_);
    // SC stores a wrapping int32 phase; use uint32 here so the wrap is
    // defined (same two's-complement bits) and UBSan stays clean.
    phase_ += static_cast<uint32_t>(static_cast<int32_t>(cpstoinc_ * freq_));
    return z;
  }
  void ProcessBlock(float* out, int n) {
    for (int i = 0; i < n; ++i) out[i] = Process();
  }
  uint32_t phase() const { return phase_; }

 private:
  const KitTables* t_ = nullptr;
  float cpstoinc_ = 0.0f;    // SC: float cpstoinc = unit->m_cpstoinc (double)
  int32_t lomask_ = 0;
  uint32_t phase_ = 0;
  float freq_ = 0.0f;
};

// LFTri (LFUGens.cpp:654-677, 3.14.1). iphase default 0; double phase;
// 4/sr phase rate. The 3.14.1 ctor primes one step and then RESETS the
// phase (m_phase = initPhase) — the first rendered sample is at phase 0.
struct LFTri {
  void Init(double sr, float freq) {
    freqmul_ = static_cast<float>(4.0 * (1.0 / sr));
    phase_ = 0.0;
    freq_ = freq;
  }
  void SetFreq(float f) { freq_ = f; }
  float Process() {
    const float z = phase_ > 1.0 ? static_cast<float>(2.0 - phase_)
                                 : static_cast<float>(phase_);
    phase_ += freq_ * freqmul_;
    if (phase_ >= 3.0) phase_ -= 4.0;
    return z;
  }
  void ProcessBlock(float* out, int n) {
    for (int i = 0; i < n; ++i) out[i] = Process();
  }

 private:
  double phase_ = 0.0;
  float freqmul_ = 0.0f;
  float freq_ = 0.0f;
};

}  // namespace sc
#endif
