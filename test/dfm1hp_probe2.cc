#include <cstdio>
#include <vector>
#include "picocollider/sc_defs.h"
#include "picocollider/sc_kit_osc2.h"
#include "picocollider/sc_kit_dfm1.h"
#include "wav_io.h"
static void render(sc::Dfm1& u, const std::vector<float>& in, std::vector<float>& out) {
  for (int b = 0; b < 750; ++b) u.ProcessBlock(in.data()+b*64, out.data()+b*64, 64);
}
int main() {
  static sc::KitTables kt; sc::InitKitTables(&kt);
  static float tbl[2*(8192+1)]; sc::BuildPulseTables(&kt, tbl);
  std::vector<float> in(48000);
  { sc::Saw src; src.Init(&kt, 48000.0, 220.0f);
    for (int b = 0; b < 750; ++b) src.ProcessBlock(in.data()+b*64, 64); }
  long epoch = 0; FILE* sf = fopen("ref/ugen_ref/DFM1hp.seed.txt", "r");
  if (sf) { if (fscanf(sf, "%ld", &epoch) != 1) epoch = 0; fclose(sf); }
  std::vector<float> out(48000);
  // V1: prime n=1, input 0.0, actual params (type=1)
  { sc::Dfm1 u; u.Init(48000.0, (int)epoch, 1000.f, 0.1f, 1.f, 1.f, 0.0003f);
    const float z = 0.f; float sink; u.ProcessBlock(&z, &sink, 1);
    render(u, in, out); l8test::WriteWavF32("build/hp_v1.wav", out.data(), 48000, 1, 48000); }
  // V2: prime n=1, input = in[0]
  { sc::Dfm1 u; u.Init(48000.0, (int)epoch, 1000.f, 0.1f, 1.f, 1.f, 0.0003f);
    float sink; u.ProcessBlock(&in[0], &sink, 1);
    render(u, in, out); l8test::WriteWavF32("build/hp_v2.wav", out.data(), 48000, 1, 48000); }
  // V3: no prime (baseline)
  { sc::Dfm1 u; u.Init(48000.0, (int)epoch, 1000.f, 0.1f, 1.f, 1.f, 0.0003f);
    render(u, in, out); l8test::WriteWavF32("build/hp_v3.wav", out.data(), 48000, 1, 48000); }
  printf("done\n");
  return 0;
}
