# PicoCollider

Reusable **SuperCollider DSP building blocks** for RP2350 / RP2040 (Cortex-M33/M0+)
projects. This started as the Dronecaster porting kit in `mi-rp2350` and is now a
standalone library so other Pico2 projects can consume it as a git submodule.

## What it is

- **26 header-only C++17 UGens/helpers** in `include/picocollider/` — noise, LFO,
  triggers, oscillators, filters, delays, reverbs, pan, demand, buffers, chaos,
  dynamics, envelopes, and classlib expansion helpers.
- **129 reference-exact NRT renders** (`test/ref/ugen_ref/`) rendered from SC
  3.14.1; the host gate compares every port at `max_abs ≤ 1e-6, corr ≥ 0.999999`.
- Host tests use **plain g++** — no MCU headers needed (the portability proof).

## Host test

```sh
cd test
make test          # builds and runs the 129-ref gate
make refs          # re-render missing references (needs sclang + sc3-plugins)
```

Expected: `test_kit: PASS (0 failures)`.

## Use from a PlatformIO project

```ini
lib_deps = PicoCollider
```
or as a git submodule:
```sh
git submodule add https://github.com/schrab/PicoCollider.git PicoCollider
```
then add `PicoCollider/include` to your project's `-I` paths and
`#include "picocollider/sc_kit_filter.h"` etc.

## Design constraints

- Faithful, **bit-exact** ports of SC 3.14.1 / stmlib / sc3-plugins sources;
  `file:line` anchors in each header.
- No libm in per-sample paths except where the source itself uses it per block.
- Single shared `sc::RGen` (taus88) — no `rand()`.
- Cortex-M33: FPU is single-precision; `double` state is kept only where the
  source's semantics require it for bit-exactness. FZ|DN must be set globally.

See [`LICENSE.md`](LICENSE.md) for the per-source license matrix.
