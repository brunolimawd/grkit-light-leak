/* Five procedural film leaks. Stock photos are visual references only. */
#include "render.h"

#define FRAME (256 * F)
#define LAYERS 5U
#define STRIP 64U
#define RUN 256U

/* Each ribbon is a curved cross-section in depth from the entry side.
   Its envelope along that side can localize it or let it span the frame. */
typedef struct {
    short depth;
    u16 width, center, length, peak;
    short cb, cr;
} Ribbon;
typedef struct { Ribbon ribbon[LAYERS]; } Reference;

static const Reference references[] = {
    /* Burnt Edge: broad red bleed, irregular bright seam and yellow corner. */
    {{{  2, 80, 124, 178, 236, -58, 168},
      { -5, 45, 127, 191, 230, -125, 68},
      { -3, 22,  30,  92, 394, -24,  17},
      {  5, 25, 240,  57, 250, -51,  31},
      { 48, 23, 204,  53,  20,  95, -29}}},
    /* Film Band: pale strip lifted off the edge, with a red lip, cool
       bleed on the inside and uneven exposed sections along its length. */
    {{{ 45, 26, 121, 168, 220, -53, 160},
      { 62, 21, 127, 186, 300, -99,  34},
      { 68, 11, 106, 150, 235, -16,   3},
      { 90, 43, 213, 112,  71,  45, -105},
      { 54, 29,  27,  17, 130, -50, 116}}},
    /* Amber Veil: luminous gold at entry with a broad, diagonal rust tail. */
    {{{ 40, 99, 143, 160, 182, -64, 117},
      { 24, 73, 154, 109, 215, -103, 42},
      {  7, 46, 157,  71, 190, -52,  15},
      { 74, 60,  52,  84, 105, -20, 124},
      {115, 57,  23,  71,  32,  27,  48}}},
    /* Prismatic Arc: overlapping red, gold, turquoise and blue across
       the curve, plus a faint cool reflection farther into the frame. */
    {{{ 46, 34, 124, 141, 137, -54, 164},
      { 62, 26, 126, 138, 165, -111, 48},
      { 79, 25, 127, 141, 124,  22, -98},
      { 94, 30, 132, 145, 113, 134, -31},
      {126, 31, 204,  57,  25, 101,  55}}},
    /* Color Fog: broad translucent teal/gold through the frame; rose
       and violet sit to either side rather than following brightness. */
    {{{ 27, 82, 145, 188,  94, -72,  46},
      { 64, 75, 146, 171,  91,  46, -77},
      {104, 79, 154, 166,  61,  93,  -6},
      { -9, 80,  58, 114,  66,  59,  96},
      {103, 97, 241,  74,  45, -12,  62}}},
};
_Static_assert(GRKIT_COUNT(references) == LOOK_FOG - LOOK_EDGE + 1, "one field per reference style");

/* Variations 2 and 3 use their own emitters and curve, instead of scaling
   the original. The bend sets the along-edge pivot, offset, slope, and bow. */
typedef struct { short pivot, bias, tilt, bow; } Bend;
typedef struct { Reference light; Bend bend; } Alternate;
static const Alternate alternates[][2] = {
    { /* Burnt Edge: localized pale seam / deep copper burn with a plum halo. */
        {{{{  4,  45,  54, 102, 265,  -52, 174},
           { -4,  23,  48,  85, 420, -106,  56},
           {  4,  13,  24,  61, 540,  -15,   8},
           {  0,  26, 205,  67,  58,  -61, 134},
           { 22,  28, 112,  58,  40,   45,  87}}}, {48, 0, -9, -4}},
        {{{{ 16, 112, 165, 151, 285,  -53, 178},
           { 14,  64, 183, 115, 240, -108,  63},
           {  7,  32, 222,  73, 410,  -35,  19},
           { 70,  53, 119,  85,  96,  102,  56},
           { -4,  30,  24,  71, 158,  -68,  57}}}, {174, 2, 22, -4}},
    },
    { /* Film Band: two red edge flashes / deeper continuous gold and cyan. */
        {{{{ 28,  17, 127, 171,  64,  -37, 147},
           { 33,  20,  55,  48, 340,  -82, 141},
           { 25,  12,  53,  32, 190,  -35, 153},
           { 34,  20, 187,  51, 350,  -51, 176},
           { 42,  15, 202,  36, 310,  -95,  50}}}, {128, 0, 10, -3}},
        {{{{ 73,  24, 138, 152, 198,  -41, 159},
           { 93,  24, 127, 170, 271, -108,  43},
           {106,  12, 119, 140, 205,  -28,   4},
           {127,  39, 145, 137, 118,   55, -118},
           {151,  30,  66,  70,  52,  108, -32}}}, {128, 0, 48, 7}},
    },
    { /* Amber Veil: compact honey-colored pool / long diagonal rust sweep. */
        {{{{ 19,  73,  69,  83, 149,  -66, 117},
           {  9,  50,  62,  71, 268, -119,  36},
           { -3,  32,  57,  42, 240,  -47,  11},
           { 42,  54, 111,  71,  73,  -25, 119},
           { 21,  34, 139,  47,  39,  -49,  59}}}, {70, 4, 55, 3}},
        {{{{ 38,  90, 146, 159, 183,  -54, 126},
           { 19,  70,  76,  94, 207, -110,  53},
           {  2,  42,  53,  50, 185,  -65,  22},
           { 72,  69, 183, 100, 147,  -20, 148},
           { 94,  56, 213,  75,  56,   37,  79}}}, {104, 8, 151, 3}},
    },
    { /* Prismatic Arc: tight, cool crescent / wide gold arc with a blue echo. */
        {{{{ 19,  19, 174,  94,  87,  -24, 154},
           { 32,  18, 175, 105, 120, -107,  29},
           { 45,  20, 174, 105, 155,   34, -118},
           { 60,  23, 173, 104, 140,  143, -19},
           { 76,  20, 201,  64,  56,  102,  61}}}, {176, 14, -24, -33}},
        {{{{ 39,  32,  83, 112, 129,  -54, 164},
           { 57,  32,  87, 130, 193, -121,  43},
           { 77,  25,  89, 132, 105,   19, -94},
           { 96,  32,  88, 126, 116,  129, -24},
           {144,  24, 105,  76,  48,   98,  54}}}, {86, 24, 53, -12}},
    },
    { /* Color Fog: sea-green and violet / rose and gold on the opposite slant. */
        {{{{ 15,  82,  95, 157, 106,   66, -112},
           { 58,  81,  94, 145,  86,  105,  -34},
           { 95,  77, 116, 135,  74,  107,   49},
           {  6,  68, 206,  98,  35,  -31,   63},
           {126,  65,  51,  96,  31,   73,   55}}}, {100, 4, -126, 3}},
        {{{{ 17,  90, 136, 177, 106,   52, 114},
           { 62,  87, 136, 153,  96,   81,  87},
           {102,  88, 152, 142,  86,  -63,  47},
           {143,  67, 143, 104,  42,   52, -58},
           {  7,  80,  58, 114,  39,  -57,  63}}}, {137, 3, 168, 2}},
    },
};
_Static_assert(GRKIT_COUNT(alternates) == GRKIT_COUNT(references), "one pair of variations per reference");

typedef struct { s32 axis; u32 first, last; u16 inv, peak; short cb, cr; } Cross;
typedef struct {
    Cross layer[LAYERS];
    u32 first, last, count;
} Section;
typedef struct { u16 across[LAYERS], along[LAYERS], radius[LAYERS]; } Kernels;
typedef struct { u32 first, step, count, reverse; } DepthGrid;
typedef struct {
    PaintTable paint;
    /* Analytic, one-dimensional cross-section, not an image-space mask. */
    u16 profile[Q12 + 1];
    Section columns[STRIP];
    u16 energy[RUN];
    s32 cb[RUN], cr[RUN];
} ReferenceWork;
_Static_assert(sizeof(ReferenceWork) + _Alignof(ReferenceWork) - 1 <= GRKIT_PLANE_SCRATCH,
    "reference field fits in scratch");

static s32 bend_at(u32 look, s32 along)
{
    s32 t = along - 128 * F;
    /* Square before shifting. Rounding along to a whole frame unit first
       makes the curve move in steps on the full-size photo. */
    u32 square = (u32)(t * t);
    switch (look) {
    case LOOK_EDGE: {
        /* Smooth irregularities of the burnt seam. Larger waves frame
           the dark center; small variations break up the white rim. */
        static const signed char ripple[] = {14, 9, 11, -1, 3, -7, 5, -3, 8, 4, 11, 5, 13, 11, 18, 14, 15};
        u32 i = (u32)along >> 12;
        if (i >= 16) return ripple[16] * F;
        s32 f = (s32)ease_phase((u32)along & 4095U);
        return (ripple[i] * (4096 - f) + ripple[i + 1] * f) >> 4;
    }
    case LOOK_BAND: return (t * 5 >> 8) + (s32)(square >> 20);
    case LOOK_AMBER: return (-t * 133 >> 8) + (s32)((square * 3) >> 18);
    case LOOK_PRISM: return 38 * F - (s32)(square >> 15) - (t * 64 >> 8);
    case LOOK_FOG: return (-t * 219 >> 8) - (s32)(square >> 19);
    }
    return 0;
}

static s32 alternate_bend(const Bend *bend, s32 along)
{
    s32 t = along - bend->pivot * F;
    /* A shifted pivot can cover more than half the frame. Keep the square
       unsigned until it has been scaled back to frame units. */
    u32 distance = (u32)(t < 0 ? -t : t);
    s32 square = (s32)((distance * distance) >> 16);
    return bend->bias * F + ((t * bend->tilt) >> 8) + ((square * bend->bow) >> 4);
}

static void prepare_section(Section *out, const Reference *ref, const Kernels *k, u32 look,
    const Bend *alternate, s32 along, const DepthGrid *depth)
{
    s32 bend = alternate ? alternate_bend(alternate, along) : bend_at(look, along);
    s32 ripple = alternate && look == LOOK_EDGE ? bend_at(look, along) : 0;
    out->first = depth->count;
    out->last = out->count = 0;
    for (u32 i = 0; i < LAYERS; ++i) {
        const Ribbon *r = &ref->ribbon[i];
        s32 shift = bend;
        if (look == LOOK_EDGE && alternate) {
            if (i == 1 || i == 2) shift += ripple / 2;
        } else if (look == LOOK_EDGE) {
            /* Keep the small waves mostly on the bright seam. The broad
               red spill curves around it; the cold echo is independent. */
            s32 t = along - 128 * F;
            if (i == 0) shift = ((t * t) >> 16) / 5 - 3 * F;
            if (i == 1) shift = bend / 2;
            if (i == 4) shift = 0;
        }
        u32 peak = mul12(r->peak * 16, bell12(along - r->center * F, k->along[i]));
        s32 axis = r->depth * F + shift;
        s32 lo = axis - k->radius[i], hi = axis + k->radius[i] + 1;
        if (!peak || hi <= 0 || lo >= FRAME) continue;
        if (lo < 0) lo = 0;
        if (hi > FRAME) hi = FRAME;
        /* With reversed depth, the floor of the screen coordinate must
           be greater than FRAME - hi and at most FRAME - lo. */
        u32 lower = (u32)(depth->reverse ? FRAME - hi + 1 : lo) << 8;
        u32 upper = (u32)(depth->reverse ? FRAME - lo + 1 : hi) << 8;
        u32 first = at_boundary(lower, depth->first, depth->step, depth->count);
        u32 last = at_boundary(upper, depth->first, depth->step, depth->count);
        if (first >= last) continue;
        out->layer[out->count++] = (Cross){axis, first, last, k->across[i], (u16)peak, r->cb, r->cr};
        if (first < out->first) out->first = first;
        if (last > out->last) out->last = last;
    }
}

/* A span is sampled at full resolution. Its emitter parameters stay in
   registers and the exact support was clipped once, outside the hot loop. */
static inline __attribute__((always_inline)) void shade_span(ReferenceWork *work, const Section *section,
    u32 first, u32 last, u32 phase, u32 step, u32 color)
{
    for (u32 i = 0; i < last - first; ++i) {
        work->energy[i] = 0;
        if (color) work->cb[i] = work->cr[i] = 0;
    }
    for (u32 i = 0; i < section->count; ++i) {
        const Cross *c = &section->layer[i];
        u32 start = c->first > first ? c->first : first;
        u32 end = c->last < last ? c->last : last;
        if (start >= end) continue;
        for (u32 at = start - first, pos = phase + start * step; at < end - first; ++at, pos += step) {
            s32 delta = (s32)(pos >> 8) - c->axis;
            u32 distance = (u32)(delta < 0 ? -delta : delta);
            u32 q = (distance * c->inv + 2048) >> 12;
            u32 e = mul12(c->peak, work->profile[q]);
            work->energy[at] += (u16)e;
            if (color) {
                work->cb[at] += (s32)e * c->cb;
                work->cr[at] += (s32)e * c->cr;
            }
        }
    }
}

static inline __attribute__((always_inline)) void reference_pass(
    const Plane *p, ReferenceWork *work, const Reference *ref, const Kernels *k,
    u32 look, u32 entry, u32 chroma, const Bend *alternate)
{
    SampleGrid s;
    if (!sample_grid(&s, p, chroma)) return;
    /* Right is the canonical field. Mirror on Left, turn to Top or
       Bottom; depth always grows inwards from the chosen entry side. */
    u32 vertical = entry == ENTRY_TOP || entry == ENTRY_BOTTOM;
    u32 reverse = entry == ENTRY_RIGHT || entry == ENTRY_BOTTOM;
    DepthGrid depth = {vertical ? s.first_y : s.first_x, vertical ? s.dy : s.dx,
        vertical ? s.rows : s.count, reverse};
    u32 along_first = vertical ? s.first_x : s.first_y;
    u32 along_step = vertical ? s.dx : s.dy;
    u32 along_count = vertical ? s.count : s.rows;
    u32 phase = reverse ? (1U << 24) + 255 - depth.first : depth.first;
    u32 step = reverse ? -depth.step : depth.step;
    u32 stride = vertical ? s.sy * p->pitch : s.sx;
    u32 chroma_stride = vertical ? p->pitch : s.sx;
    /* Left/Right draw one row at a time. Top/Bottom tile columns and
       work on only 64 adjacent rows at a time for image-cache locality. */
    u32 group = vertical ? STRIP : 1;
    u32 run = vertical ? 64 : RUN;
    for (u32 block = 0; block < along_count; block += group) {
        u32 count = along_count - block < group ? along_count - block : group;
        u32 lo = depth.count, hi = 0;
        for (u32 i = 0, pos = along_first + block * along_step; i < count; ++i, pos += along_step) {
            s32 along = (s32)(pos >> 8);
            Section *section = &work->columns[i];
            prepare_section(section, ref, k, look, alternate, entry == ENTRY_BOTTOM ? FRAME - along : along, &depth);
            if (section->first < lo) lo = section->first;
            if (section->last > hi) hi = section->last;
        }
        for (u32 j = lo; j < hi; j += run) {
            u32 end = hi - j < run ? hi : j + run;
            for (u32 i = 0; i < count; ++i) {
                const Section *section = &work->columns[i];
                u32 first = j > section->first ? j : section->first;
                u32 last = end < section->last ? end : section->last;
                if (first >= last) continue;
                shade_span(work, section, first, last, phase, step, chroma);
                u32 col = s.left + (vertical ? block + i : first) * s.sx;
                u32 y = s.top + (vertical ? first : block + i) * s.sy;
                volatile u8 *luma = GRKIT_BYTES(p->y + y * p->pitch + col);
                if (chroma) {
                    volatile u8 *c = GRKIT_BYTES(chroma_row(p, y) + col);
                    const volatile u8 *below = s.sy == 2 ? luma + p->pitch : luma;
                    for (u32 n = 0, offset = 0, cp = 0; n < last - first; ++n, offset += stride, cp += chroma_stride) {
                        Exposure light = {work->energy[n], work->cb[n], work->cr[n]};
                        paint_c(&work->paint, &light, 0, luma + offset, below + offset, c + cp);
                    }
                } else {
                    for (u32 n = 0, offset = 0; n < last - first; ++n, offset += stride)
                        paint_y(&work->paint, work->energy[n], luma + offset);
                }
            }
        }
    }
}

void reference_leak_apply(PictureSet *set, u32 look, u32 strength, u32 entry, u32 variant)
{
    u32 index = look - LOOK_EDGE;
    const Alternate *shape = variant == VARIANT_FIRST ? 0 : &alternates[index][variant - VARIANT_SECOND];
    const Reference *ref = shape ? &shape->light : &references[index];
    const Bend *bend = shape ? &shape->bend : 0;
    u32 max_energy = 0;
    for (u32 i = 0; i < LAYERS; ++i) max_energy += ref->ribbon[i].peak * 16U;
    if (max_energy > EXPOSURE_LIMIT) return;
    __UINTPTR_TYPE__ address = (__UINTPTR_TYPE__)set->scratch;
    u32 pad = (u32)(-address & (_Alignof(ReferenceWork) - 1));
    ReferenceWork *work = (ReferenceWork *)(void *)(set->scratch + pad);
    prepare_paint(&work->paint, strength, max_energy);
    prepare_profile(work->profile);
    Kernels k;
    for (u32 i = 0; i < LAYERS; ++i) {
        k.across[i] = inverse_radius(ref->ribbon[i].width * F);
        k.along[i] = inverse_radius(ref->ribbon[i].length * F);
        /* Last Q8 distance for which the rounded Q12 index is in range. */
        k.radius[i] = (u16)divide((1U << 24) - 2049, k.across[i]);
    }
    for (u32 i = 0; i < set->count; ++i) {
        const Plane *p = &set->planes[i];
        if (!visible_plane(p)) continue;
        /* Tint reads the original luma, to protect clipped highlights. */
        if (!set->mono && p->c) reference_pass(p, work, ref, &k, look, entry, 1, bend);
        reference_pass(p, work, ref, &k, look, entry, 0, bend);
    }
}
