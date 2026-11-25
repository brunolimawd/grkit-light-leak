#ifndef LIGHT_LEAK_RENDER_H
#define LIGHT_LEAK_RENDER_H

#include "exposure.h"

/* Coordinates are Q8 of a 256-unit frame, envelopes are Q12, and light
   energy has four fractional bits. Every layer samples at native centers. */
#define F 256
#define Q12 4096U
#define GAIN_STEPS 256U
#define GAIN_SHIFT 7U /* one step is 8 units of exposure, represented in Q4 */
#define EXPOSURE_LIMIT (28U * 1024U)

typedef struct { u16 value[GAIN_STEPS + 1U]; } GainTable;
/* Low half: luma lift in Q16. High half: chroma gain in Q19. Index by the
   exact accumulated exposure, not by a pixel or a reduced image grid. */
typedef struct { u32 value[EXPOSURE_LIMIT + 1U]; } PaintTable;

static inline u32 mul12(u32 a, u32 b) { return (a * b + 2048) >> 12; }
static inline u32 ease_phase(u32 q) { return mul12(mul12(q, q), 3 * Q12 - 2 * q); }

static inline u32 ease(s32 x)
{
    if (x <= 0) return 0;
    if (x >= 256 * F) return Q12;
    return ease_phase(((u32)x + 8) >> 4);
}

static inline u32 bell12(s32 delta, u32 inverse)
{
    u32 d = (u32)(delta < 0 ? -delta : delta);
    u32 t = (d * inverse + 2048) >> 12;
    if (t >= Q12) return 0;
    u32 q = Q12 - mul12(t, t);
    return mul12(q, q);
}

static inline u16 inverse_radius(u32 radius)
{
    return (u16)divide((1U << 24) + (radius >> 1), radius);
}

static inline void prepare_gain(GainTable *gain, u32 strength)
{
    for (u32 i = 0; i <= GAIN_STEPS; ++i) {
        u32 d = 32768 + 8 * i * strength;
        gain->value[i] = (u16)divide((strength << 23) + (d >> 1), d);
    }
}

/* The shoulder curve is a lookup in exposure space, not a raster mask. */
static inline u32 exposure_gain(const GainTable *gain, u32 energy)
{
    u32 i = energy >> GAIN_SHIFT;
    if (i >= GAIN_STEPS) return gain->value[GAIN_STEPS];
    u32 f = energy & 127U;
    return (gain->value[i] * (128 - f) + gain->value[i + 1] * f + 64) >> GAIN_SHIFT;
}

static inline void prepare_paint(PaintTable *paint, u32 strength, u32 max_energy)
{
    GainTable curve;
    prepare_gain(&curve, strength);
    for (u32 e = 0; e <= max_energy; ++e) {
        u32 gain = exposure_gain(&curve, e);
        u32 lift = (e * gain + 1024) >> 11;
        paint->value[e] = lift | (((gain + 8) >> 4) << 16);
    }
}

static inline void prepare_profile(u16 *profile)
{
    for (u32 i = 0; i < Q12; ++i) {
        u32 v = Q12 - mul12(i, i);
        profile[i] = (u16)mul12(v, v);
    }
    profile[Q12] = 0;
}

static inline u32 profile_at(const u16 *profile, s32 delta, u32 inverse)
{
    u32 distance = (u32)(delta < 0 ? -delta : delta);
    u32 q = (distance * inverse + 2048) >> 12;
    return profile[q < Q12 ? q : Q12];
}

static inline u8 chroma_value(u8 base, s32 offset, u32 lift, u32 room)
{
    s32 delta = (((128 - (s32)base) * (s32)lift + 128) >> 8) + offset;
    s32 out = base + ((delta * (s32)room + 32768) >> 16);
    return (u8)(out < 0 ? 0 : out > 255 ? 255 : out);
}

static inline void paint_chroma(volatile u8 *c, u32 y, const Exposure *light, s32 color_gain, u32 lift, u32 trace)
{
    u32 room = 255 - y;
    u32 headroom = room + ((room * y) >> 8);
    if (trace) {
        /* Let the cold trace cross midtones. Preserve clipped white. */
        u32 q = room >= 64 ? 256 : room * 4;
        u32 keep = (q * (512 - q) + 128) >> 8;
        if (keep > headroom) headroom += mul12(keep - headroom, trace);
    }
    /* Chroma gain was reduced to Q19 when building the paint table,
       keeping the signed products in range; output tint is Q8. */
    c[0] = chroma_value(c[0], (light->cb * color_gain + 16384) >> 15, lift, headroom);
    c[1] = chroma_value(c[1], (light->cr * color_gain + 16384) >> 15, lift, headroom);
}

static inline void paint_y(const PaintTable *paint, u32 energy, volatile u8 *y)
{
    if (!energy) return;
    u32 lift = (u16)paint->value[energy];
    u32 base = *y;
    *y = (u8)(base + (((255 - base) * lift + 32768) >> 16));
}

static inline void paint_c(const PaintTable *paint, const Exposure *light, u32 trace,
    const volatile u8 *luma, const volatile u8 *below, volatile u8 *chroma)
{
    if (!light->energy) return;
    u32 gain = paint->value[light->energy];
    u32 y4 = luma[0] + luma[1] + below[0] + below[1];
    paint_chroma(chroma, (y4 + 2) >> 2, light, (s32)(gain >> 16), (u16)gain, trace);
}

/* First sample at or beyond a boundary in normalized Q24 coordinates. */
static inline u32 at_boundary(u32 bound, u32 first, u32 step, u32 count)
{
    if (bound <= first) return 0;
    u32 i = divide(bound - first + step - 1, step);
    return i < count ? i : count;
}

/* Y uses pixel centers. CbCr uses the center of each complete 2x1 or 2x2
   footprint, leaving shared samples that cross a crop edge untouched. */
typedef struct { u32 sx, sy, left, top, count, rows, dx, dy, first_x, first_y; } SampleGrid;

static inline u32 sample_grid(SampleGrid *s, const Plane *p, u32 chroma)
{
    u32 sx = chroma ? 2 : 1, sy = chroma && p->half ? 2 : 1;
    u32 left = chroma ? (p->left + 1) & ~1U : p->left;
    u32 top = sy == 2 ? (p->top + 1) & ~1U : p->top;
    u32 count = (p->right - left) >> (sx == 2), rows = (p->bottom - top) >> (sy == 2);
    if (!count || !rows) return 0;

    u32 dx = divide(1U << 24, p->right - p->left), dy = divide(1U << 24, p->bottom - p->top);
    u32 first_x = (left - p->left) * dx + ((sx * dx) >> 1);
    u32 first_y = (top - p->top) * dy + ((sy * dy) >> 1);
    s->sx = sx; s->sy = sy; s->left = left; s->top = top;
    s->count = count; s->rows = rows;
    s->dx = dx * sx; s->dy = dy * sy; s->first_x = first_x; s->first_y = first_y;
    return 1;
}

static inline u32 visible_plane(const Plane *p)
{
    return p->y && p->left < p->right && p->top < p->bottom &&
        p->right <= p->width && p->bottom <= p->height && p->width <= p->pitch;
}

#endif
