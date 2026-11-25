#ifndef LIGHT_LEAK_EXPOSURE_H
#define LIGHT_LEAK_EXPOSURE_H

#include "grkit.h"

/* Add light before applying the film-like shoulder. Cb and Cr are sums of
   the chroma offsets from neutral, weighted by the energy of each source. */
typedef struct { u32 energy; s32 cb, cr; } Exposure;

static inline void expose(Exposure *out, u32 energy, s32 cb, s32 cr)
{
    out->energy += energy;
    out->cb += (s32)energy * cb;
    out->cr += (s32)energy * cr;
}

static inline void expose_sample(Exposure *out, u32 energy, s32 cb, s32 cr, u32 color)
{
    if (color) expose(out, energy, cb, cr);
    else out->energy += energy;
}

enum { ENTRY_RIGHT, ENTRY_LEFT, ENTRY_TOP, ENTRY_BOTTOM, ENTRY_RANDOM, ENTRY_ORIGINAL };
/* Keep in the same order as the LOOK rows and tiles. */
enum { LOOK_OFF, LOOK_RANDOM, LOOK_SLIT, LOOK_EDGE, LOOK_BAND, LOOK_AMBER, LOOK_PRISM, LOOK_FOG };
enum { VARIANT_FIRST, VARIANT_SECOND, VARIANT_THIRD, VARIANT_AUTO };

/* Render the whole Slit Flare directly into the JPEG planes. */
void slit_apply(PictureSet *set, u32 strength, u32 entry, u32 variant);
/* Render the other reference looks with the same per-sample exposure model. */
void reference_leak_apply(PictureSet *set, u32 look, u32 strength, u32 entry, u32 variant);

#endif
