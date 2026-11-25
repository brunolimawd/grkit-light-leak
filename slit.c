/* Slit Flare. The curtain, beam, and internal reflections have different
   cross sections: a wavy edge, a tapering streak, and soft detached ghosts. */
#include "render.h"

/* The effect is evaluated at the centers of the actual Y and CbCr samples.
   Coordinates are Q8 of a 256-unit frame; distance and envelopes have 12
   fractional bits, and the exposures have 4. No raster mask is enlarged. */
#define BEAM_MIN 24U
#define BEAM_MAX 108U
#define WALL_MIN 146U  /* minimum of the edge profile */
#define STRIP 256U
/* Sum of the beam peaks (1180 + 243 + 88 + 38 + at most 138
   of cool light). The wall is composited separately and is weaker. */
#define SLIT_ENERGY_BOUND (1700U * 16)
_Static_assert(SLIT_ENERGY_BOUND <= EXPOSURE_LIMIT, "slit intensity fits in paint table");
typedef struct {
    u16 axis, gold_axis, core_inv, rim_inv, trace_inv;
    short cold_offset;
    u16 core, rim, blue, cyan, trace, red_ghost, gold_ghost;
    u16 first, last;
} Ray;
typedef struct { u16 x, red_keep, amber, corner; } WallDepth;
typedef struct { u16 edge, red, amber, corner; } WallEdge;
typedef struct {
    PaintTable paint;
    u16 falloff[Q12];
    u16 profile[Q12 + 1];
    union {
        Ray ray[STRIP];
        WallDepth depth[STRIP];
        WallEdge edge[STRIP];
    } columns;
} SlitWork;
_Static_assert(sizeof(SlitWork) + _Alignof(SlitWork) - 1 <= GRKIT_PLANE_SCRATCH, "slit work fits in scratch");

typedef struct {
    u8 axis, gold_axis, width, taper, rim_start, core_start, red, amber, cold;
    signed char slope, reflection, cold_offset, wall;
} SlitVariant;
static const SlitVariant variants[] = {
    /* Original white beam, curtain and reflections. */
    { 83,  75,  8, 12, 103, 164, 128, 128, 128,  31,   0,   0,   0},
    /* Short honey-colored cut across the middle, with a shallow curtain. */
    {146, 139,  4,  9, 141, 183,  88, 157,  75,  45,  23,   0,  24},
    /* Wide diagonal beam, deeper red curtain and a detached blue trail. */
    { 94,  86, 11, 17,  72, 128, 121,  89, 192, -59, -21, -21, -19},
};
_Static_assert(GRKIT_COUNT(variants) == VARIANT_AUTO, "one beam per fixed variation");

static u32 wall_profile(const u8 *knots, u32 v)
{
    u32 i = v >> 13;
    if (i == 8) return knots[i] * F;
    u32 phase = ease((s32)((v & 8191U) << 3));
    /* Keep the fractional position of the red contour and its brightness
       along the edge instead of rounding them to whole profile units. */
    return (knots[i] * (Q12 - phase) + knots[i + 1] * phase + 8) >> 4;
}

static void prepare_wall_edge(WallEdge *e, s32 v, const SlitVariant *variant)
{
    static const u8 edge[] = {153, 162, 149, 155, 169, 158, 163, 149, WALL_MIN};
    static const u8 heat[] = {178, 145, 133, 211, 230, 170, 218, 240, 203};
    e->edge = (u16)((s32)wall_profile(edge, (u32)v) + variant->wall * F);
    u32 red = (wall_profile(heat, (u32)v) + 8) >> 4;
    u32 amber = 160 * 16 + mul12(bell12(v - 238 * F, 65536U / 45), 74 * 16);
    e->red = (u16)((red * variant->red) >> 7);
    e->amber = (u16)((amber * variant->amber) >> 7);
    e->corner = (u16)bell12(v - 249 * F, 65536U / 28);
}

static void prepare_wall_depth(WallDepth *d, s32 x)
{
    d->x = (u16)x;
    d->red_keep = (u16)(Q12 - (ease(7 * (x - 221 * F)) >> 1));
    d->amber = (u16)ease(7 * (x - 198 * F));
    d->corner = (u16)ease(12 * (x - 230 * F));
}

static inline void wall_light(const SlitWork *work, const WallDepth *d, const WallEdge *e, Exposure *light, u32 color)
{
    s32 inside = (s32)d->x - e->edge;
    if (inside <= 0) return; /* The amber and corner sources start closer to the entry edge. */
    u32 phase = (5 * (u32)inside + 8) >> 4;
    u32 falloff = phase >= Q12 ? Q12 : work->falloff[phase];
    u32 red = mul12(mul12(falloff, d->red_keep), e->red);
    expose_sample(light, red, -71, 181, color);
    expose_sample(light, mul12(d->amber, e->amber), -91, 33, color);
    expose_sample(light, mul12(d->corner, e->corner), -38, 15, color);
}

static inline void include_ray(Ray *r, u32 center, u32 radius)
{
    /* Include a small margin for rounding the inverse radius. */
    u32 first = center - radius - F / 16, last = center + radius + F / 16;
    if (first < r->first) r->first = (u16)first;
    if (last > r->last) r->last = (u16)last;
}

/* A Ray describes one exact coordinate down the length of the beam. It is
   reused for the column (Left/Right) or row (Top/Bottom) that crosses it. */
static void prepare_ray(Ray *r, s32 x, u32 vertical, const SlitVariant *variant)
{
    r->axis = (u16)(variant->axis * F - ((x * variant->slope) >> 8));
    r->gold_axis = (u16)(variant->gold_axis * F);
    r->cold_offset = (short)(variant->cold_offset * F);
    r->first = BEAM_MAX * F;
    r->last = BEAM_MIN * F;
    u32 width = (u32)(variant->width * F + ((x * variant->taper) >> 8));
    r->core_inv = inverse_radius(width);
    r->rim_inv = inverse_radius(width + 7 * F);
    r->rim = (u16)mul12(243 * 16, ease(3 * (x - variant->rim_start * F)));
    /* Keep the bright core pale when the broad curtain is added afterward. */
    r->core = (u16)mul12(1180 * 16, ease(3 * (x - variant->core_start * F)));
    if (r->rim || r->core) include_ray(r, r->axis, width + 7 * F);
    s32 reflection = x - variant->reflection * F;
    u32 cool_axis = (u32)((s32)r->axis + r->cold_offset);
    u32 cold = (61 * 16 * variant->cold) >> 7;
    if (vertical) {
        r->trace = (u16)mul12(ease(5 * (reflection - 40 * F)), ease(6 * (175 * F - reflection)));
        u32 radius = 8 * F + ((u32)x >> 6);
        r->trace_inv = inverse_radius(radius);
        if (r->trace) include_ray(r, cool_axis - F, radius);
        u32 teal = ease(4 * (reflection - 67 * F));
        cold = mul12(r->trace, cold);
        r->blue = (u16)mul12(cold, Q12 - teal);
        r->cyan = (u16)mul12(cold, teal);
    } else {
        r->blue = (u16)mul12(bell12(reflection - 85 * F, 65536U / 46), (31 * 16 * variant->cold) >> 7);
        r->cyan = (u16)mul12(bell12(reflection - 119 * F, 65536U / 45), cold);
        if (r->blue) include_ray(r, cool_axis - F, 10 * F);
        if (r->cyan) include_ray(r, cool_axis - 2 * F, 11 * F);
    }
    r->red_ghost = (u16)mul12(bell12(reflection - 140 * F, 65536U / 25), 88 * 16);
    r->gold_ghost = (u16)mul12(bell12(reflection - 26 * F, 65536U / 13), 38 * 16);
    if (r->red_ghost) include_ray(r, r->axis + 10 * F, 16 * F);
    if (r->gold_ghost) include_ray(r, r->gold_axis, 15 * F);
}

static inline u32 ray_light(const u16 *profile, const Ray *r, s32 y, u32 vertical, Exposure *light, u32 color)
{
    s32 delta = y - r->axis;
    if (r->rim) expose_sample(light, mul12(r->rim, profile_at(profile, delta, r->rim_inv)), -33, 53, color);
    if (r->core) expose_sample(light, mul12(r->core, profile_at(profile, delta, r->core_inv)), -6, 1, color);
    u32 trace = 0;
    s32 cold = delta - r->cold_offset;
    if (vertical) {
        if (r->trace) {
            u32 cross = profile_at(profile, cold + F, r->trace_inv);
            expose_sample(light, mul12(r->blue, cross), 93, -35, color);
            expose_sample(light, mul12(r->cyan, cross), 55, -100, color);
            if (color) trace = mul12(r->trace, cross);
        }
    } else {
        if (r->blue)
            expose_sample(light, mul12(r->blue, profile_at(profile, cold + F, 65536U / 10)), 93, -35, color);
        if (r->cyan)
            expose_sample(light, mul12(r->cyan, profile_at(profile, cold + 2 * F, 65536U / 11)), 55, -100, color);
    }
    if (r->red_ghost)
        expose_sample(light, mul12(r->red_ghost, profile_at(profile, delta - 10 * F, 65536U / 16)), -24, 104, color);
    if (r->gold_ghost)
        expose_sample(light, mul12(r->gold_ghost, profile_at(profile, y - r->gold_axis, 65536U / 15)), -38, 66, color);
    return trace;
}

static inline __attribute__((always_inline)) void detail_pass(
    const Plane *p, SlitWork *work, u32 entry, u32 chroma, const SlitVariant *variant)
{
    SampleGrid s;
    if (!sample_grid(&s, p, chroma)) return;
    u32 vertical = entry == ENTRY_TOP || entry == ENTRY_BOTTOM;
    u32 count = s.count, rows = s.rows, dx = s.dx, dy = s.dy, first_x = s.first_x, first_y = s.first_y;
    /* Horizontal entries cache the rays for a strip of columns and scan
       rows contiguously. Vertical entries only need one ray per row.
       Skip outside the beam's support, not just outside its intensity. */
    for (u32 block = 0; block < count;) {
        u32 last = vertical || count - block <= STRIP ? count : block + STRIP;
        u32 row0 = 0, row1 = rows;
        if (!vertical) {
            u32 lo = BEAM_MAX * F, hi = BEAM_MIN * F;
            for (u32 i = block, x = first_x + block * dx; i < last; ++i, x += dx) {
                s32 coord = (s32)(x >> 8);
                Ray *r = &work->columns.ray[i - block];
                prepare_ray(r, entry == ENTRY_LEFT ? 256 * F - coord : coord, 0, variant);
                if (r->first < lo) lo = r->first;
                if (r->last > hi) hi = r->last;
            }
            row0 = at_boundary(lo << 8, first_y, dy, rows);
            row1 = at_boundary(hi << 8, first_y, dy, rows);
        }
        for (u32 j = row0, pos = first_y + row0 * dy; j < row1; ++j, pos += dy) {
            s32 coord = (s32)(pos >> 8);
            Ray ray;
            u32 start = block, end = last;
            if (vertical) {
                prepare_ray(&ray, entry == ENTRY_TOP ? 256 * F - coord : coord, 1, variant);
                u32 lo = entry == ENTRY_TOP ? ray.first : 256 * F - ray.last;
                u32 hi = entry == ENTRY_TOP ? ray.last : 256 * F - ray.first;
                start = at_boundary(lo << 8, first_x, dx, count);
                end = at_boundary(hi << 8, first_x, dx, count);
            }
            if (start >= end) continue;
            u32 y = s.top + j * s.sy, col = s.left + start * s.sx;
            volatile u8 *luma = GRKIT_BYTES(p->y + y * p->pitch + col);
            volatile u8 *c = chroma ? GRKIT_BYTES(chroma_row(p, y) + col) : 0;
            const volatile u8 *below = s.sy == 2 ? luma + p->pitch : luma;
            for (u32 i = start, x = first_x + start * dx, k = 0; i < end; ++i, x += dx, k += s.sx) {
                const Ray *r = vertical ? &ray : &work->columns.ray[i - block];
                s32 cross = !vertical ? coord : entry == ENTRY_TOP ? (s32)(x >> 8) : 256 * F - (s32)(x >> 8);
                Exposure light = {0, 0, 0};
                u32 trace = ray_light(work->profile, r, cross, vertical, &light, chroma);
                if (chroma) paint_c(&work->paint, &light, trace, luma + k, below + k, c + k);
                else paint_y(&work->paint, light.energy, luma + k);
            }
        }
        block = last;
    }
}

static inline __attribute__((always_inline)) void wall_pass(
    const Plane *p, SlitWork *work, u32 entry, u32 chroma, const SlitVariant *variant)
{
    SampleGrid s;
    if (!sample_grid(&s, p, chroma)) return;
    u32 vertical = entry == ENTRY_TOP || entry == ENTRY_BOTTOM;
    u32 first = 0, end = s.count, row0 = 0, row1 = s.rows;
    u32 wall_min = (u32)((s32)WALL_MIN + variant->wall);
    switch (entry) {
    case ENTRY_RIGHT: first = at_boundary(wall_min << 16, s.first_x, s.dx, end); break;
    case ENTRY_LEFT: end = at_boundary((256 - wall_min) << 16, s.first_x, s.dx, end); break;
    case ENTRY_TOP: row1 = at_boundary((256 - wall_min) << 16, s.first_y, s.dy, row1); break;
    case ENTRY_BOTTOM: row0 = at_boundary(wall_min << 16, s.first_y, s.dy, row1); break;
    }
    for (u32 block = first; block < end;) {
        u32 last = end - block <= STRIP ? end : block + STRIP;
        for (u32 i = block, x = s.first_x + block * s.dx; i < last; ++i, x += s.dx) {
            s32 coord = (s32)(x >> 8);
            if (vertical)
                prepare_wall_edge(&work->columns.edge[i - block], entry == ENTRY_BOTTOM ? 256 * F - coord : coord, variant);
            else
                prepare_wall_depth(&work->columns.depth[i - block], entry == ENTRY_LEFT ? 256 * F - coord : coord);
        }
        for (u32 j = row0, pos = s.first_y + row0 * s.dy; j < row1; ++j, pos += s.dy) {
            s32 coord = (s32)(pos >> 8);
            WallDepth depth;
            WallEdge edge;
            if (vertical) prepare_wall_depth(&depth, entry == ENTRY_TOP ? 256 * F - coord : coord);
            else prepare_wall_edge(&edge, coord, variant);
            u32 y = s.top + j * s.sy, col = s.left + block * s.sx;
            volatile u8 *luma = GRKIT_BYTES(p->y + y * p->pitch + col);
            volatile u8 *c = chroma ? GRKIT_BYTES(chroma_row(p, y) + col) : 0;
            const volatile u8 *below = s.sy == 2 ? luma + p->pitch : luma;
            for (u32 i = 0, k = 0; i < last - block; ++i, k += s.sx) {
                Exposure light = {0, 0, 0};
                wall_light(work, vertical ? &depth : &work->columns.depth[i],
                    vertical ? &work->columns.edge[i] : &edge, &light, chroma);
                if (chroma) paint_c(&work->paint, &light, 0, luma + k, below + k, c + k);
                else paint_y(&work->paint, light.energy, luma + k);
            }
        }
        block = last;
    }
}

void slit_apply(PictureSet *set, u32 strength, u32 entry, u32 variant)
{
    const SlitVariant *shape = &variants[variant];
    /* Reuse the same strip storage for the ray and curtain parameters. */
    __UINTPTR_TYPE__ address = (__UINTPTR_TYPE__)set->scratch;
    u32 pad = (u32)(-address & (_Alignof(SlitWork) - 1));
    SlitWork *work = (SlitWork *)(void *)(set->scratch + pad);
    prepare_paint(&work->paint, strength, SLIT_ENERGY_BOUND);
    prepare_profile(work->profile);
    /* This table is the one-dimensional easing curve, not a light mask.
       The phase is computed at every sample from the fractional edge. */
    for (u32 q = 0; q < Q12; ++q) work->falloff[q] = (u16)ease_phase(q);
    for (u32 i = 0; i < set->count; ++i) {
        const Plane *p = &set->planes[i];
        if (!visible_plane(p)) continue;
        /* Tint first in each layer. Putting the curtain last lets
           highlight protection keep its red off the white beam. */
        u32 color = !set->mono && p->c;
        if (color) detail_pass(p, work, entry, 1, shape);
        detail_pass(p, work, entry, 0, shape);
        if (color) wall_pass(p, work, entry, 1, shape);
        wall_pass(p, work, entry, 0, shape);
    }
}
