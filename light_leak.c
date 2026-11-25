/* Light Leak: a film-like exposure from the edge of the visible frame.
   The leak stays tied to the landscape sensor axes, as on film. */
#include "grkit_module.h"
#include "render.h"

static const u8 strengths[] = {51, 114, 225};
/* Map the native-resolution reference styles to their default entry side. */
static const u8 original[] = {ENTRY_RIGHT, ENTRY_RIGHT, ENTRY_BOTTOM, ENTRY_TOP, ENTRY_RIGHT, ENTRY_TOP};
_Static_assert(GRKIT_COUNT(original) == LOOK_FOG - LOOK_SLIT + 1U, "one entry per reference look");

/* Seed each encode from the capture data and samples of its largest valid
   plane. As with Film-like Grain, use the shot's existing metadata, not a
   fresh firmware clock call. Sensor noise adds variation within a second.
   Keep all generator state on the stack, not in the module's data segment. */
static u32 shot_seed(const PictureSet *set)
{
    const Plane *main = 0;
    u32 area = 0;
    for (u32 i = 0; i < set->count; ++i) {
        const Plane *p = &set->planes[i];
        if (!visible_plane(p)) continue;
        u32 size = (p->right - p->left) * (p->bottom - p->top);
        if (size > area) { main = p; area = size; }
    }

    u32 seed = 0x4c45414bU;
    if (!main) return seed;
    GrkitShot shot;
    shot.size = sizeof(shot);
    shot.known = 0;
    picture_shot(set, &shot);
    if (shot.known & GRKIT_SHOT_TAKEN) {
        u32 day = ((u32)shot.taken.year * 12 + shot.taken.month) * 31 + shot.taken.day;
        u32 seconds = ((day * 24 + shot.taken.hour) * 60 + shot.taken.minute) * 60 + shot.taken.second;
        seed = (seed ^ seconds) * 0x01000193U;
    }
    if (shot.known & GRKIT_SHOT_EXPOSURE)
        seed = (seed ^ shot.exposure_us ^ ((u32)shot.sv << 16) ^ (u32)shot.av) * 0x01000193U;

    u32 w = main->right - main->left, h = main->bottom - main->top;
    for (u32 j = 1; j < 16; j += 2) {
        u32 row = main->top + ((h * j) >> 4);
        const volatile u8 *y = GRKIT_BYTES(main->y + row * main->pitch + main->left);
        for (u32 i = 1; i < 16; i += 2)
            seed = (seed ^ y[(w * i) >> 4]) * 0x01000193U;
    }
    return seed;
}

/* Mix each successive word and scale it to [0, count) in bounded time. */
static u32 random_choice(u32 *state, u32 count)
{
    u32 word = (*state += 0x9e3779b9U);
    word = (word ^ (word >> 16)) * 0x85ebca6bU;
    word = (word ^ (word >> 13)) * 0xc2b2ae35U;
    word ^= word >> 16;
    return (u32)(((unsigned long long)word * count) >> 32);
}

void light_leak_apply(PictureSet *set, u32 value)
{
    if (!set->scratch || !set->count || set->count > GRKIT_COUNT(set->planes) ||
        value == LOOK_OFF || value > LOOK_FOG) return;
    u32 random_look = value == LOOK_RANDOM;
    u32 level = random_look ? 1 : setting_get(SETTING_STRENGTH);
    u32 entry = random_look ? ENTRY_RANDOM : setting_get(SETTING_POSITION);
    u32 variant = setting_get(SETTING_VARIATION);
    if (level >= GRKIT_COUNT(strengths)) level = 1;
    if (variant > VARIANT_AUTO) variant = VARIANT_FIRST;
    if (entry == ENTRY_RANDOM || variant == VARIANT_AUTO) {
        /* One seed and one selection for the whole encode, including previews. */
        u32 state = shot_seed(set);
        if (random_look) {
            value = LOOK_SLIT + random_choice(&state, GRKIT_COUNT(original));
            level = random_choice(&state, GRKIT_COUNT(strengths));
        }
        if (entry == ENTRY_RANDOM) entry = random_choice(&state, ENTRY_RANDOM);
        if (variant == VARIANT_AUTO) variant = random_choice(&state, VARIANT_AUTO);
    }
    if (entry > ENTRY_BOTTOM) entry = original[value - LOOK_SLIT];
    if (value == LOOK_SLIT) slit_apply(set, strengths[level], entry, variant);
    else reference_leak_apply(set, value, strengths[level], entry, variant);
}
