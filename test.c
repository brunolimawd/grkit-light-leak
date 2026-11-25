#include "grkit_test.h"
#include "grkit_module.h"
#include "exposure.h"
#include <assert.h>

void light_leak_apply(PictureSet *set, u32 value);
void light_leak_badge(const Surface *s, u32 value);

#define GRAY 72U
#define NEUTRAL 128U
enum { RANDOM = LOOK_RANDOM, SLIT = LOOK_SLIT };
enum { RIGHT = ENTRY_RIGHT, LEFT = ENTRY_LEFT, TOP = ENTRY_TOP, BOTTOM = ENTRY_BOTTOM, RANDOM_POSITION = ENTRY_RANDOM };

static Plane plane_on(u32 slot, u32 w, u32 h, u32 pitch, u32 half)
{
    u32 y = GRKIT_TEST_PLANES + slot * 0x10000U;
    return (Plane){y, y + pitch * h, w, h, pitch, half, 0, 0, w, h};
}

static volatile u8 *luma(const Plane *p, u32 x, u32 y)
{
    return GRKIT_BYTES(p->y + y * p->pitch + x);
}

static volatile u8 *chroma(const Plane *p, u32 x, u32 y)
{
    return GRKIT_BYTES(chroma_row(p, y) + x);
}

static void fill(const Plane *p)
{
    volatile u8 *y = GRKIT_BYTES(p->y), *c = GRKIT_BYTES(p->c);
    for (u32 i = 0; i < p->pitch * p->height; ++i) y[i] = GRAY;
    u32 chroma_height = p->half ? (p->height + 1) >> 1 : p->height;
    for (u32 i = 0; i < p->pitch * chroma_height; ++i) c[i] = NEUTRAL;
}

static void check_guard(const Plane *p, u32 off, u32 mono)
{
    for (u32 y = 0; y < p->height; ++y)
        for (u32 x = 0; x < p->pitch; ++x)
            if (off || x < p->left || x >= p->right || y < p->top || y >= p->bottom)
                assert(*luma(p, x, y) == GRAY);
    u32 rows = p->half ? 2 : 1;
    for (u32 y = 0; y < p->height; y += rows)
        for (u32 x = 0; x < p->pitch; x += 2)
            if (off || mono || x < p->left || x + 1 >= p->right || y < p->top || y + rows > p->bottom) {
                assert(*chroma(p, x, y) == NEUTRAL);
                assert(*chroma(p, x + 1, y) == NEUTRAL);
            }
}

static void check_off(void)
{
    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    Plane *p = &set.planes[0];
    *p = plane_on(0, 96, 64, 104, 0);
    fill(p);
    light_leak_apply(&set, LOOK_OFF);
    light_leak_apply(&set, LOOK_FOG + 1);
    check_guard(p, 1, 0);
    set.count = GRKIT_COUNT(set.planes) + 1;
    light_leak_apply(&set, RANDOM);
    check_guard(p, 1, 0);
}

static void check_planes(void)
{
    PictureSet set = {.count = 3, .scratch = grkit_test_scratch};
    set.planes[0] = plane_on(0, 160, 120, 168, 1);
    set.planes[1] = plane_on(1, 80, 60, 84, 0);
    set.planes[2] = plane_on(2, 40, 30, 44, 1);
    setting_set(SETTING_STRENGTH, 1);
    setting_set(SETTING_POSITION, RIGHT);
    /* EXIF rotation must not move a leak that's tied to the sensor. */
    for (set.turns = 0; set.turns < 4; ++set.turns) {
        for (u32 i = 0; i < set.count; ++i) fill(&set.planes[i]);
        light_leak_apply(&set, LOOK_EDGE);
        for (u32 i = 0; i < set.count; ++i) {
            Plane *p = &set.planes[i];
            const Plane *full = &set.planes[0];
            u32 x = p->width * 9 / 10, y = p->height / 3, scale = full->width / p->width;
            u32 sum = 0, area = scale * scale;
            /* A thumbnail pixel and a main-image pixel have different
               centers. Compare the thumbnail against its full footprint. */
            for (u32 v = 0; v < scale; ++v)
                for (u32 u = 0; u < scale; ++u)
                    sum += *luma(full, x * scale + u, y * scale + v);
            u32 main = (sum + area / 2) / area, scaled = *luma(p, x, y);
            assert(scaled > GRAY + 15 && scaled + 6 >= main && scaled <= main + 6);
            assert(*luma(p, p->width / 4, p->height / 10) == GRAY);
            assert(*luma(p, 0, p->height - 1) == GRAY);
            check_guard(p, 0, 0);
        }
    }
}

static void check_slit(void)
{
    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    Plane *p = &set.planes[0];
    *p = plane_on(0, 128, 128, 132, 1);
    setting_set(SETTING_POSITION, RIGHT);

    u32 previous = GRAY;
    for (u32 level = 0; level < 3; ++level) {
        fill(p);
        setting_set(SETTING_STRENGTH, level);
        light_leak_apply(&set, SLIT);
        u32 core = *luma(p, 121, 26);
        assert(core > previous);
        assert(core > *luma(p, 121, 44) + 34);
        previous = core;
        check_guard(p, 0, 0);
    }
    assert(previous > 225);

    fill(p);
    setting_set(SETTING_STRENGTH, 1);
    light_leak_apply(&set, SLIT);
    /* A saturated red curtain, an almost neutral beam, and a distinctly
       colder reflection separated from the entry edge. */
    assert(*chroma(p, 101, 78) > NEUTRAL + 22);
    assert(*chroma(p, 121, 26) < *chroma(p, 101, 78));
    assert(*chroma(p, 54, 34) > NEUTRAL + 3 && *chroma(p, 55, 34) < NEUTRAL - 3);
    assert(*luma(p, 24, 90) == GRAY);

    static const u8 core_at[][2] = {{121, 26}, {6, 26}, {26, 6}, {101, 121}};
    for (u32 i = 0; i < GRKIT_COUNT(core_at); ++i) {
        setting_set(SETTING_POSITION, i);
        fill(p);
        light_leak_apply(&set, SLIT);
        assert(*luma(p, core_at[i][0], core_at[i][1]) > 200);
        assert(*luma(p, core_at[i ^ 1U][0], core_at[i ^ 1U][1]) < 120);
        check_guard(p, 0, 0);
    }
    setting_set(SETTING_POSITION, RIGHT);
}

static void check_slit_trace(u32 half)
{
    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    Plane *p = &set.planes[0];
    *p = plane_on(0, 160, 112, 168, half);
    setting_set(SETTING_STRENGTH, 1);
    const u32 coords[][2] = {{42, 62}, {116, 48}};

    for (u32 i = 0; i < GRKIT_COUNT(coords); ++i) {
        u32 x = coords[i][0], y = coords[i][1];
        setting_set(SETTING_POSITION, TOP + i);
        fill(p);
        light_leak_apply(&set, SLIT);
        u8 cb = *chroma(p, x, y), cr = *chroma(p, x + 1, y);
        assert(cb >= NEUTRAL + 8 && cr <= NEUTRAL - 10);
        assert(*chroma(p, 80, y) == NEUTRAL);

        /* Cross a narrow, light object on an otherwise dark scene. The
           geometry stays put and the object no longer cuts the cyan trace. */
        fill(p);
        for (u32 row = y; row < y + 2; ++row)
            for (u32 col = 0; col < p->width; ++col)
                *luma(p, col, row) = 180;
        light_leak_apply(&set, SLIT);
        assert(*chroma(p, x, y) + 4 >= cb && *chroma(p, x + 1, y) <= cr + 4);
        assert(*chroma(p, 80, y) == NEUTRAL);

        /* A fully clipped pair still does not take any tint. */
        fill(p);
        for (u32 row = y; row < y + 2; ++row)
            for (u32 col = x; col < x + 2; ++col)
                *luma(p, col, row) = 255;
        light_leak_apply(&set, SLIT);
        assert(*luma(p, x, y) == 255);
        assert(*chroma(p, x, y) == NEUTRAL && *chroma(p, x + 1, y) == NEUTRAL);
        check_guard(p, 0, 0);
    }
}

/* The first moment of the cold trace across the beam, in Q8 pixels. The
   low threshold ignores near-neutral quantization at the very edges. */
static u32 slit_center(const Plane *p, u32 fixed, u32 first, u32 last, u32 vertical)
{
    u32 sum = 0, weight = 0;
    for (u32 i = first; i < last; i += 2) {
        const volatile u8 *c = vertical ? chroma(p, i, fixed) : chroma(p, fixed, i);
        s32 blue = c[0] - c[1] - 3;
        if (blue > 0) {
            sum += (i - first + 1) * (u32)blue;
            weight += (u32)blue;
        }
    }
    assert(weight);
    return divide(sum << 8, weight);
}

static void check_slit_sampling(void)
{
    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    Plane *p = &set.planes[0];
    *p = plane_on(0, 1800, 1200, 1800, 1);
    setting_set(SETTING_STRENGTH, 1);

    /* A sufficiently large flat field exposes the former staircase: the
       beam used to hold its center, then jump multiple pixels between
       nearby samples. Check smooth progress in both axes. */
    fill(p);
    setting_set(SETTING_POSITION, RIGHT);
    light_leak_apply(&set, SLIT);
    u32 old = slit_center(p, 706, 260, 410, 0);
    for (u32 x = 718; x <= 826; x += 12) {
        u32 next = slit_center(p, x, 260, 410, 0);
        assert(next + 128 <= old && old <= next + 640);
        old = next;
    }

    fill(p);
    setting_set(SETTING_POSITION, BOTTOM);
    light_leak_apply(&set, SLIT);
    old = slit_center(p, 470, 1240, 1400, 1);
    for (u32 y = 478; y <= 550; y += 8) {
        u32 next = slit_center(p, y, 1240, 1400, 1);
        assert(old + 192 <= next && next <= old + 704);
        old = next;
    }
}

/* Measure the warm contour across several Cr levels rather than relying on
   the position of one 8-bit step. Return the mean crossing in Q8 pixels. */
static u32 wall_contour(const Plane *p, u32 fixed, u32 first, u32 last, u32 vertical)
{
    static const u8 levels[] = {136, 141, 146, 151, 156, 161};
    u32 sum = 0, level = 0, prev = NEUTRAL;
    for (u32 i = first; i < last && level < GRKIT_COUNT(levels); i += 2) {
        u32 cr = vertical ? *chroma(p, fixed + 1, i) : *chroma(p, i + 1, fixed);
        while (level < GRKIT_COUNT(levels) && cr >= levels[level]) {
            assert(cr > prev && levels[level] > prev);
            sum += ((i - 1) << 8) + divide((levels[level] - prev) << 9, cr - prev);
            ++level;
        }
        prev = cr;
    }
    assert(level == GRKIT_COUNT(levels));
    return sum / GRKIT_COUNT(levels);
}

static void check_wall_sampling(void)
{
    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    Plane *p = &set.planes[0];
    *p = plane_on(0, 1800, 1200, 1800, 1);
    setting_set(SETTING_STRENGTH, 1);

    /* These segments are away from the narrow beam and have a steadily
       moving red contour. The former curtain grid jumped or even moved
       back on short stretches of this same test field. */
    fill(p);
    setting_set(SETTING_POSITION, RIGHT);
    light_leak_apply(&set, SLIT);
    u32 prev = wall_contour(p, 637, 1000, 1450, 0);
    for (u32 y = 643; y <= 721; y += 6) {
        u32 next = wall_contour(p, y, 1000, 1450, 0);
        assert(next + 384 <= prev && prev <= next + 1280);
        prev = next;
    }

    fill(p);
    setting_set(SETTING_POSITION, BOTTOM);
    light_leak_apply(&set, SLIT);
    prev = wall_contour(p, 698, 680, 1050, 1);
    for (u32 x = 706; x <= 810; x += 8) {
        u32 next = wall_contour(p, x, 680, 1050, 1);
        assert(prev + 128 <= next && next <= prev + 896);
        prev = next;
    }
}

static u32 plane_hash(const Plane *p)
{
    u32 hash = 2166136261U, ch = p->half ? (p->height + 1) >> 1 : p->height;
    const volatile u8 *y = GRKIT_BYTES(p->y), *c = GRKIT_BYTES(p->c);
    for (u32 n = 0; n < p->pitch * p->height; ++n) hash = (hash ^ y[n]) * 16777619U;
    for (u32 n = 0; n < p->pitch * ch; ++n) hash = (hash ^ c[n]) * 16777619U;
    return hash;
}

static void check_v10_rendering(void)
{
    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    Plane *p = &set.planes[0];
    *p = plane_on(0, 96, 64, 96, 1);
    /* The changed menu IDs must still produce the same Y and CbCr. */
    static const u32 v10[][ENTRY_RANDOM] = {
        {0xa6eff05b, 0x8615090f, 0xe8ca2a25, 0xe4221bec},
        {0xbbd8f649, 0xe2d78e52, 0xe3979269, 0x127d30b7},
        {0xbc72aa68, 0x92398bcb, 0x54456d85, 0x28d9d5fc},
        {0x7c3e51c4, 0xd426603b, 0x1d3ae239, 0xa898f7f4},
        {0x55f8b94c, 0xd0a7d519, 0x492e28da, 0x834d5921},
        {0x7ef294f8, 0xe084fc57, 0x9e58e5b7, 0x6ac234b5},
    };
    setting_set(SETTING_STRENGTH, 1);
    for (u32 i = 0; i < GRKIT_COUNT(v10); ++i)
        for (u32 entry = RIGHT; entry <= BOTTOM; ++entry) {
            setting_set(SETTING_POSITION, entry);
            fill(p);
            light_leak_apply(&set, SLIT + i);
            assert(plane_hash(p) == v10[i][entry]);
        }
}

static void check_reference_shapes(void)
{
    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    Plane *p = &set.planes[0];
    *p = plane_on(0, 128, 128, 128, 1);
    setting_set(SETTING_STRENGTH, 1);
    setting_set(SETTING_POSITION, RIGHT);

    fill(p);
    light_leak_apply(&set, LOOK_EDGE);
    assert(*luma(p, 126, 15) > 175 && *luma(p, 108, 64) > 110);
    assert(*chroma(p, 109, 64) > 160 && *luma(p, 64, 64) == GRAY);

    fill(p);
    light_leak_apply(&set, LOOK_BAND);
    assert(*luma(p, 126, 54) == GRAY && *luma(p, 95, 54) > 175);
    assert(*chroma(p, 101, 54) > 160 && *chroma(p, 94, 54) < 110);
    assert(*chroma(p, 80, 104) > NEUTRAL && *chroma(p, 81, 104) < NEUTRAL);

    fill(p);
    light_leak_apply(&set, LOOK_AMBER);
    assert(*luma(p, 126, 81) > 175 && *luma(p, 70, 29) > 130);
    assert(*luma(p, 80, 31) > *luma(p, 80, 77) + 30);
    assert(*chroma(p, 126, 81) < 100 && *chroma(p, 71, 29) > 150);

    fill(p);
    light_leak_apply(&set, LOOK_PRISM);
    /* Similar luma, with red and blue on different sides of the arc. */
    assert(*luma(p, 82, 57) > 145 && *luma(p, 64, 57) > 140);
    assert(*chroma(p, 83, 57) > 160 && *chroma(p, 64, 57) > 145);
    assert(*luma(p, 126, 57) == GRAY && *luma(p, 126, 7) > GRAY);

    fill(p);
    light_leak_apply(&set, LOOK_FOG);
    assert(*chroma(p, 121, 59) > NEUTRAL && *chroma(p, 91, 62) < NEUTRAL);
    assert(*luma(p, 91, 62) > 130 && *luma(p, 54, 87) == GRAY);
}

static void close_sample(u32 a, u32 b)
{
    assert(a <= b + 1 && b <= a + 1);
}

static void check_reference_positions(void)
{
    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    Plane *p = &set.planes[0];
    *p = plane_on(0, 96, 96, 104, 1);
    const Plane right = plane_on(1, 96, 96, 104, 1);
    static const u8 sides[] = {RIGHT, RIGHT, BOTTOM, TOP, RIGHT, TOP};
    for (u32 style = SLIT; style <= LOOK_FOG; ++style) {
        fill(p);
        setting_set(SETTING_STRENGTH, 1);
        setting_set(SETTING_POSITION, ENTRY_ORIGINAL);
        light_leak_apply(&set, style);
        u32 hash = plane_hash(p);
        fill(p);
        setting_set(SETTING_POSITION, sides[style - SLIT]);
        light_leak_apply(&set, style);
        assert(plane_hash(p) == hash);
        if (style == SLIT) continue; /* v9 has a separate beam model in Top and Bottom */

        /* A square makes the native Y and 4:2:0 sample centers coincide
           after the transform. Allow one value for fixed-point rounding. */
        PictureSet baseline = {.planes = {right}, .count = 1, .scratch = grkit_test_scratch};
        fill(&right);
        setting_set(SETTING_POSITION, RIGHT);
        light_leak_apply(&baseline, style);
        for (u32 entry = LEFT; entry <= BOTTOM; ++entry) {
            fill(p);
            setting_set(SETTING_POSITION, entry);
            light_leak_apply(&set, style);
            for (u32 y = 0; y < 96; ++y)
                for (u32 x = 0; x < 96; ++x) {
                    u32 col = entry == LEFT ? 95 - x : entry == TOP ? y : 95 - y;
                    u32 row = entry == LEFT ? y : entry == TOP ? 95 - x : x;
                    close_sample(*luma(&right, x, y), *luma(p, col, row));
                    if (!((x | y) & 1)) {
                        for (u32 c = 0; c < 2; ++c)
                            close_sample(*chroma(&right, x + c, y), *chroma(p, (col & ~1U) + c, row));
                    }
                }
        }
    }
}

static void check_variation_separation(void)
{
    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    enum { WIDTH = 128, HEIGHT = 96, AREA = WIDTH * HEIGHT };
    Plane samples[VARIANT_AUTO];
    for (u32 v = VARIANT_FIRST; v < VARIANT_AUTO; ++v)
        samples[v] = plane_on(v, WIDTH, HEIGHT, WIDTH, 1);
    setting_set(SETTING_STRENGTH, 1);
    for (u32 look = SLIT; look <= LOOK_FOG; ++look)
        for (u32 side = RIGHT; side <= BOTTOM; ++side) {
            setting_set(SETTING_POSITION, side);
            for (u32 v = VARIANT_FIRST; v < VARIANT_AUTO; ++v) {
                set.planes[0] = samples[v];
                fill(&samples[v]);
                setting_set(SETTING_VARIATION, v);
                light_leak_apply(&set, look);
            }
            for (u32 v = VARIANT_SECOND; v < VARIANT_AUTO; ++v)
                for (u32 earlier = VARIANT_FIRST; earlier < v; ++earlier) {
                    const volatile u8 *a = GRKIT_BYTES(samples[earlier].y), *b = GRKIT_BYTES(samples[v].y);
                    u32 brighter = 0, darker = 0;
                    for (u32 i = 0; i < AREA; ++i) {
                        s32 d = b[i] - a[i];
                        brighter += d >= 12;
                        darker += d <= -12;
                    }
                    /* The difference must be visible across the frame and
                       change where the light lands, not just overall gain. */
                    assert((brighter + darker) * 100 >= 14 * AREA);
                    assert(brighter * 100 >= AREA && darker * 100 >= AREA);
                }
        }
    setting_set(SETTING_VARIATION, VARIANT_FIRST);
}

static void check_reference_planes(u32 half, u32 variant)
{
    PictureSet set = {.count = 1, .scratch = grkit_test_scratch + 1};
    Plane *p = &set.planes[0];
    *p = plane_on(0, 126, 88, 134, half);
    p->left = 5;
    p->right = 117;
    p->top = 3;
    p->bottom = 83;
    setting_set(SETTING_VARIATION, variant);
    for (u32 style = SLIT; style <= LOOK_FOG; ++style) {
        for (u32 entry = RIGHT; entry <= BOTTOM; ++entry) {
            setting_set(SETTING_POSITION, entry);
            u32 last = 0;
            for (u32 level = 0; level < 3; ++level) {
                setting_set(SETTING_STRENGTH, level);
                fill(p);
                light_leak_apply(&set, style);
                u32 sum = 0, clear = 0;
                for (u32 y = p->top; y < p->bottom; ++y)
                    for (u32 x = p->left; x < p->right; ++x) {
                        u32 value = *luma(p, x, y);
                        assert(value >= GRAY);
                        sum += value;
                        clear |= value == GRAY;
                    }
                assert(sum > last && clear);
                last = sum;
                check_guard(p, 0, 0);
            }
            fill(p);
            set.mono = 1;
            light_leak_apply(&set, style);
            check_guard(p, 0, 1);
            set.mono = 0;
        }

        /* Clipped white and its original chroma are unchanged throughout. */
        fill(p);
        for (u32 i = 0; i < p->pitch * p->height; ++i) GRKIT_BYTES(p->y)[i] = 255;
        for (u32 i = 0; i < p->pitch * p->height / (half ? 2 : 1); ++i) GRKIT_BYTES(p->c)[i] = 132;
        u32 hash = plane_hash(p);
        light_leak_apply(&set, style);
        assert(hash == plane_hash(p));
    }
    setting_set(SETTING_VARIATION, VARIANT_FIRST);
}

static void check_veil_sampling(void)
{
    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    Plane *p = &set.planes[0];
    *p = plane_on(0, 1800, 1200, 1800, 1);
    fill(p);
    setting_set(SETTING_STRENGTH, 1);
    setting_set(SETTING_POSITION, TOP);
    light_leak_apply(&set, LOOK_AMBER);
    /* Track several isophotes through the diagonal tail. Averaging their
       positions reduces the effect of the final eight-bit quantization. */
    u32 previous = 0;
    for (u32 x = 430; x <= 542; x += 8) {
        u32 y = 850, sum = 0;
        for (u32 level = 84; level <= 132; level += 8) {
            while (y > 560 && *luma(p, x, y) < level) --y;
            assert(y > 560 && y < 850);
            sum += y;
        }
        if (previous) assert(sum + 31 <= previous && previous <= sum + 51);
        previous = sum;
    }
}

static void capture_at(u32 seconds)
{
    grkit_test_shot.known = GRKIT_SHOT_TAKEN;
    grkit_test_shot.taken = (GrkitDateTime){2026, 10, 25, 9, (u8)(seconds / 60), (u8)(seconds % 60), 0};
}

static u32 match_draw(const PictureSet *set, const u32 fixed[][3], u32 count, u32 stride)
{
    u32 hash = plane_hash(&set->planes[0]), pick = 0;
    while (pick < count && hash != fixed[pick * stride][0]) ++pick;
    assert(pick < count);
    for (u32 p = 1; p < set->count; ++p)
        assert(plane_hash(&set->planes[p]) == fixed[pick * stride][p]);
    return pick;
}

static void check_random(void)
{
    PictureSet set = {.count = 3, .scratch = grkit_test_scratch};
    set.planes[0] = plane_on(0, 96, 64, 100, 1);
    set.planes[1] = plane_on(1, 48, 32, 52, 0);
    set.planes[2] = plane_on(2, 24, 16, 28, 1);
    enum {
        LEVELS = 3, ENTRIES = ENTRY_RANDOM, VARIANTS = VARIANT_AUTO,
        PER_ENTRY = VARIANTS * LEVELS, PER_STYLE = ENTRIES * PER_ENTRY,
        COMBINATIONS = (LOOK_FOG - SLIT + 1) * PER_STYLE
    };
    u32 fixed[COMBINATIONS][3];
    for (u32 i = 0; i < COMBINATIONS; ++i) {
        setting_set(SETTING_STRENGTH, i % LEVELS);
        setting_set(SETTING_VARIATION, i / LEVELS % VARIANTS);
        setting_set(SETTING_POSITION, i / PER_ENTRY % ENTRIES);
        for (u32 p = 0; p < set.count; ++p) fill(&set.planes[p]);
        light_leak_apply(&set, SLIT + i / PER_STYLE);
        for (u32 p = 0; p < set.count; ++p) fixed[i][p] = plane_hash(&set.planes[p]);
        for (u32 j = 0; j < i; ++j) assert(fixed[i][0] != fixed[j][0]);
    }
    /* Keep the scene still and vary the capture metadata. Every
       combination is reachable; repeats are allowed. */
    setting_set(SETTING_STRENGTH, 0);
    setting_set(SETTING_POSITION, LEFT);
    setting_set(SETTING_VARIATION, VARIANT_AUTO);
    u32 previous = COMBINATIONS, repeated = 0;
    u8 seen[COMBINATIONS] = {0};
    for (u32 n = 0; n < 3000; ++n) {
        capture_at(n);
        for (u32 p = 0; p < set.count; ++p) fill(&set.planes[p]);
        light_leak_apply(&set, RANDOM);
        u32 match = match_draw(&set, fixed, COMBINATIONS, 1);
        seen[match] = 1;
        repeated |= match == previous;
        previous = match;
    }
    assert(repeated);
    for (u32 i = 0; i < COMBINATIONS; ++i) assert(seen[i]);
    assert(setting_get(SETTING_STRENGTH) == 0);
    assert(setting_get(SETTING_POSITION) == LEFT);
    assert(setting_get(SETTING_VARIATION) == VARIANT_AUTO);

    /* Global Random can lock a variation while still drawing style, side and strength. */
    setting_set(SETTING_VARIATION, VARIANT_THIRD);
    for (u32 n = 0; n < 48; ++n) {
        capture_at(n);
        for (u32 p = 0; p < set.count; ++p) fill(&set.planes[p]);
        light_leak_apply(&set, RANDOM);
        assert(match_draw(&set, fixed, COMBINATIONS, 1) / LEVELS % VARIANTS == VARIANT_THIRD);
    }

    /* Random entry alone preserves the chosen style, strength and variation. */
    setting_set(SETTING_POSITION, RANDOM_POSITION);
    for (u32 style = SLIT; style <= LOOK_FOG; ++style) {
        u32 seen_entries = 0, base = (style - SLIT) * PER_STYLE + VARIANT_THIRD * LEVELS;
        for (u32 n = 0; n < 48; ++n) {
            capture_at(n);
            for (u32 p = 0; p < set.count; ++p) fill(&set.planes[p]);
            light_leak_apply(&set, style);
            u32 entry = match_draw(&set, fixed + base, ENTRIES, PER_ENTRY);
            seen_entries |= 1U << entry;
        }
        assert(seen_entries == (1U << ENTRIES) - 1);
    }
    assert(setting_get(SETTING_POSITION) == RANDOM_POSITION);

    /* Auto alone draws all three variations without moving the entry or
       changing the intensity. Check every style and side, including previews. */
    setting_set(SETTING_VARIATION, VARIANT_AUTO);
    setting_set(SETTING_STRENGTH, 1);
    for (u32 style = SLIT; style <= LOOK_FOG; ++style)
        for (u32 side = 0; side < ENTRIES; ++side) {
            setting_set(SETTING_POSITION, side);
            u32 seen_variants = 0, base = (style - SLIT) * PER_STYLE + side * PER_ENTRY + 1;
            for (u32 n = 0; n < 32; ++n) {
                capture_at(n);
                for (u32 p = 0; p < set.count; ++p) fill(&set.planes[p]);
                light_leak_apply(&set, style);
                u32 variant = match_draw(&set, fixed + base, VARIANTS, LEVELS);
                seen_variants |= 1U << variant;
            }
            assert(seen_variants == (1U << VARIANTS) - 1);
        }
}

static void check_random_burst(void)
{
    PictureSet set = {.count = 3, .scratch = grkit_test_scratch};
    /* A malformed plane must not be sampled, even if it's listed first. */
    set.planes[0] = (Plane){.y = 1, .width = 48, .height = 32, .pitch = 48, .right = 49, .bottom = 32};
    Plane *preview = &set.planes[1], *main = &set.planes[2];
    *preview = plane_on(0, 48, 32, 52, 1);
    *main = plane_on(1, 96, 64, 100, 1);
    main->left = 5;
    main->top = 3;
    main->right = 91;
    main->bottom = 61;
    u32 x = main->left + (main->right - main->left) / 16;
    u32 y = main->top + (main->bottom - main->top) / 16;
    for (u32 known = 0; known <= 1; ++known) {
        capture_at(20);
        if (!known) grkit_test_shot.known = 0;
        u32 first = 0, varied = 0;
        for (u32 i = 0; i <= 32; ++i) {
            fill(main);
            fill(preview);
            *luma(main, x, y) += (u8)(i & 31U);
            /* Jam the test wall clock; the capture time is unchanged. */
            grkit_test_time.second = (u8)(i + 4);
            light_leak_apply(&set, RANDOM);
            u32 hash = plane_hash(preview);
            if (i == 0) first = hash;
            else if (i == 32) assert(hash == first);
            else varied |= hash != first;
            check_guard(main, 0, 0);
        }
        assert(varied);
    }
}

static u32 badge_hash(const GrkitLayer *layer, const Surface *s, u32 style, u32 level)
{
    enum { LEFT_X = 12, TOP_Y = 88, SIZE = 32 };
    enum { SYMBOL_ROWS = 20 };
    const u32 white = 0xffffffffU, gray = 0xff808080U, black = 0xff000000U;
    u32 active = style != LOOK_OFF && style <= LOOK_FOG;
    setting_set(SETTING_STRENGTH, level);
    grkit_test_clear();
    layer->paint(s, style);
    u32 hash = 2166136261U, ink = 0, outline = 0, dim = 0;
    for (u32 y = 0; y < 480; ++y)
        for (u32 x = 0; x < GRKIT_TEST_STRIDE; ++x) {
            u32 pixel = grkit_test_pixel(x, y);
            if (!active || x < LEFT_X || x >= LEFT_X + SIZE || y < TOP_Y || y >= TOP_Y + SIZE) {
                assert(pixel == GRKIT_TEST_CLEAR);
                continue;
            }
            ink += pixel == white;
            outline += pixel == black;
            dim += pixel == gray;
            assert(pixel == GRKIT_TEST_CLEAR || pixel == white || pixel == gray || pixel == black);
            if (style == LOOK_RANDOM || y < TOP_Y + SYMBOL_ROWS) hash = (hash ^ pixel) * 16777619U;
        }
    if (active) {
        assert(ink && outline);
        const u32 steps[] = {5, 15, 25};
        for (u32 i = 0; i < GRKIT_COUNT(steps); ++i)
            assert(grkit_test_pixel(LEFT_X + steps[i], TOP_Y + 28) ==
                (style == LOOK_RANDOM ? GRKIT_TEST_CLEAR : i <= level ? white : gray));
        assert(!!dim == (style != LOOK_RANDOM && level < 2));
    }
    return hash;
}

static void check_badge(void)
{
    const GrkitLayer *layer = 0;
    for (u32 i = 0; i < grkit_module.registration_count; ++i) {
        const GrkitRegistration *r = &grkit_module.registrations[i];
        assert(r->point != GRKIT_POINT_VIEWFINDER_INDICATOR);
        if (r->point == GRKIT_POINT_OVERLAY_LAYER) layer = r->impl;
    }
    assert(layer && layer->paint == light_leak_badge);
    assert(layer->flags == (GRKIT_LAYER_FULL_DISP | GRKIT_LAYER_LIVE_ONLY));
    assert(GRKIT_SETTING(layer->setting) == SETTING_LOOK);
    assert(layer->condition_count == 1);
    const GrkitCondition *c = layer->conditions;
    assert(GRKIT_SETTING(c->setting) == SETTING_ICON);
    assert(c->comparison == GRKIT_CONDITION_EQUALS && c->value == 0);
    assert(grkit_module.settings[c->setting].default_value == 0);
    assert(grkit_module.settings[SETTING_VARIATION - GRKIT_TEST_SETTING_BASE].default_value == VARIANT_AUTO);

    /* Cropping the picture must not move a screen-space indicator. */
    const u16 box[] = {120, 98, 600, 382};
    Surface s = grkit_test_surface(box);
    badge_hash(layer, &s, LOOK_OFF, 1);
    badge_hash(layer, &s, LOOK_FOG + 1, 1);
    u32 shapes[LOOK_FOG + 1] = {0};
    for (u32 style = LOOK_RANDOM; style <= LOOK_FOG; ++style) {
        shapes[style] = badge_hash(layer, &s, style, 0);
        for (u32 previous = LOOK_RANDOM; previous < style; ++previous)
            assert(shapes[style] != shapes[previous]);
        for (u32 level = 1; level < 3; ++level)
            assert(badge_hash(layer, &s, style, level) == shapes[style]);
    }
}

int main(void)
{
    check_off();
    check_planes();
    check_slit();
    check_slit_trace(0);
    check_slit_trace(1);
    check_slit_sampling();
    check_wall_sampling();
    check_v10_rendering();
    check_reference_shapes();
    check_reference_positions();
    check_variation_separation();
    for (u32 variant = VARIANT_FIRST; variant < VARIANT_AUTO; ++variant) {
        check_reference_planes(0, variant);
        check_reference_planes(1, variant);
    }
    check_veil_sampling();
    check_random();
    check_random_burst();
    check_badge();
    return 0;
}
