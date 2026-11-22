#include <cstdio>
#include <cmath>
#include <vector>
#include "picocollider/sc_defs.h"
#include "picocollider/sc_kit_noise.h"
#include "picocollider/sc_rgen.h"
#include "wav_io.h"
int main() {
  sc::RGen r; r.init(sc::kGoldenSeed);
  sc::LFNoise0 noise; noise.Init(&r, 48000.0, 400.0f);
  std::vector<float> n(48000);
  for (int b = 0; b < 750; ++b) {
    float blk[64]; noise.ProcessBlock(blk, 64);
    for (int i = 0; i < 64; ++i) n[b*64+i] = blk[i] * 64.0f + 64.0f;
  }
  std::vector<float> a(48000), b2(48000), c(48000);
  for (int i = 0; i < 48000; ++i) {
    const float x = (n[i] - 69.0f) / 12.0f;
    a[i] = 440.f * std::pow(2.f, x);          // libm powf
    b2[i] = 440.f * std::exp2f(x);            // exp2f
    c[i] = 440.f * std::exp((x) * 0.6931472f); // exp variant
  }
  l8test::WriteWavF32("build/mid_powf.wav", a.data(), 48000, 1, 48000);
  l8test::WriteWavF32("build/mid_exp2.wav", b2.data(), 48000, 1, 48000);
  l8test::WriteWavF32("build/mid_exp.wav", c.data(), 48000, 1, 48000);
  printf("done\n");
  return 0;
}
