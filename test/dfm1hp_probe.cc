#include <cstdio>
#include <vector>
#include "picocollider/sc_defs.h"
#include "picocollider/sc_kit_osc2.h"
#include "picocollider/sc_kit_dfm1.h"
#include "wav_io.h"
int main() {
  static sc::KitTables kt; sc::InitKitTables(&kt);
  static float tbl[2*(8192+1)]; sc::BuildPulseTables(&kt, tbl);
  std::vector<float> in(48000), out(48000);
  { sc::Saw src; src.Init(&kt, 48000.0, 220.0f);
    for (int b = 0; b < 750; ++b) src.ProcessBlock(in.data() + b*64, 64); }
  long epoch = 0; FILE* sf = fopen("ref/ugen_ref/DFM1hp.seed.txt", "r");
  if (sf) { if (fscanf(sf, "%ld", &epoch) != 1) epoch = 0; fclose(sf); }
  printf("epoch=%ld\n", epoch);
  sc::Dfm1 u; u.Init(48000.0, (int)epoch, 1000.0f, 0.1f, 1.0f, 1.0f, 0.0003f);
  for (int b = 0; b < 750; ++b) u.ProcessBlock(in.data()+b*64, out.data()+b*64, 64);
  l8test::WriteWavF32("build/dfm1hp_got.wav", out.data(), 48000, 1, 48000);
  return 0;
}
