# License matrix

This library aggregates ports of several upstreamsources. Each port header quotes
its source's license line. Summary:

| Group | Headers | Upstream license |
|---|---|---|
| scsynth core + classlib | most `sc_kit_*.h`, `sc_sugar.h`, `sc_kit_envgen.h` | GPL-3.0-or-later |
| Mutable Instruments stmlib | `sc_kit_osc.h`, `sc_kit_pulse.h` (tables) | GPL-3.0 |
| sc3-plugins Distortion/DEIND/MCLD | `sc_kit_sc3p_filter.h`, `sc_kit_sc3p_dyn.h`, `sc_kit_greyhole.h`, `sc_kit_dfm1.h` | GPL-2.0-or-later (headers quoted verbatim in each file) |
| Faust-generated GreyholeRaw | `sc_kit_greyhole.h` | GPL-2.0-or-later (author Julian Parker) |

The generated table file `sc_kit_dfm1_tables.h` is derived from sc3-plugins and
carries the same GPL-2+ notice.

The test reference WAVs are renders of the upstream algorithms and are provided
for verification only.
