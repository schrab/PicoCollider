#include <cstdio>
#include "picocollider/sc_defs.h"
#include "picocollider/sc_kit_osc2.h"
int main() {
  static sc::KitTables kt; sc::InitKitTables(&kt);
  static float tbl[2*(8192+1)]; sc::BuildPulseTables(&kt, tbl);
  float buf[16];
  sc::Saw src; src.Init(&kt, 48000.0, 220.0f);
  src.ProcessBlock(buf, 16);
  printf("kit Saw s0..s15: ");
  for (int i = 0; i < 16; ++i) printf("%.6g ", buf[i]);
  printf("\n");
  return 0;
}
