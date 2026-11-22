#include <cstdio>
#include <vector>
#include "picocollider/sc_defs.h"
#include "picocollider/sc_kit_osc.h"
#include "picocollider/sc_kit_osc2.h"
#include "picocollider/sc_kit_noise.h"
#include "picocollider/sc_kit_lfo.h"
#include "picocollider/sc_kit_lin.h"
#include "picocollider/sc_kit_localio.h"
#include "picocollider/sc_rgen.h"
#include "wav_io.h"
int main() {
  std::vector<float> out(48000);
  { // Midicps stream (same as the kit check)
    sc::RGen r; r.init(sc::kGoldenSeed);
    sc::LFNoise0 noise; noise.Init(&r, 750.0, 10.0f);
    sc::LinLin lin; lin.Init(-1.0f, 1.0f, 0.0f, 127.0f);
    float prev = lin.ctor_value(noise.ctor_value());
    for (int b = 0; b < 750; ++b) {
      const float v = lin.ProcessKr(noise.ProcessKr());
      const float slope = static_cast<float>((v - prev) * (1.0 / 64.0));
      const float slope2 = slope + slope;
      const float v0 = prev, v1 = prev + slope, v2 = prev + slope2,
                  v3 = prev + slope2 + slope;
      float acc = 0.0f;
      for (int g = 0; g < 16; ++g) {
        float* o = out.data() + b * 64 + g * 4;
        o[0] = sc::midicps_ar(v0 + acc);
        o[1] = sc::midicps_ar(v1 + acc);
        o[2] = sc::midicps_ar(v2 + acc);
        o[3] = sc::midicps_ar(v3 + acc);
        acc += slope2 + slope2;
      }
      prev = v;
    }
    l8test::WriteWavF32("build/midicps_got.wav", out.data(), 48000, 1, 48000);
  }
  { // LocalIO2 stream
    static sc::KitTables kt; sc::InitKitTables(&kt);
    static float tbl[2*(8192+1)]; sc::BuildPulseTables(&kt, tbl);
    sc::LocalBus bus; bus.Init(2);
    sc::LocalIn lin; lin.Init(2);
    sc::LocalOut lout; lout.Init(2);
    sc::SinOsc osc; osc.Init(&kt, 48000.0, 220.0f);
    sc::LFSaw saw; saw.Init(48000.0, 110.0f);
    const float defaults[2] = {0.f, 0.f};
    for (int b = 0; b < 750; ++b) {
      float c0[64], c1[64], w0[64], w1[64];
      float* inr[2] = {c0, c1};
      lin.ProcessBlock(bus, inr, defaults, b, 64);
      for (int i = 0; i < 64; ++i) {
        w0[i] = c0[i] * 0.5f + osc.Process() * 0.1f;
        w1[i] = c1[i] * 0.25f + saw.Next() * 0.1f;
      }
      const float* obr[2] = {w0, w1};
      lout.ProcessBlock(bus, obr, b, 64);
      for (int i = 0; i < 64; ++i) out[b * 64 + i] = c0[i] + c1[i];
    }
    l8test::WriteWavF32("build/localio2_got.wav", out.data(), 48000, 1, 48000);
  }
  printf("done\n");
  return 0;
}
