/* Live-view symbols use an overlay layer so they can coexist with the six
   native status-row registrations shared by the official modules. */
#include "grkit_module.h"
#include "exposure.h"
#include "badge_art.h"

#define BADGE_LEFT 12U
#define BADGE_TOP 88U
_Static_assert(BADGE_SIZE == 32U, "one 32-bit mask per badge row");
_Static_assert(GRKIT_COUNT(badge_symbols) == LOOK_FOG - LOOK_RANDOM + 1, "one symbol per active style");

void light_leak_badge(const Surface *s, u32 value)
{
    if (value == LOOK_OFF || value > LOOK_FOG) return;
    const u32 *shape = badge_symbols[value - LOOK_RANDOM];
    const u32 *active = 0, *scale = 0;
    if (value != LOOK_RANDOM) {
        u32 level = setting_get(SETTING_STRENGTH);
        if (level >= GRKIT_COUNT(badge_steps)) level = 1;
        active = badge_steps[level];
        scale = badge_steps[GRKIT_COUNT(badge_steps) - 1];
    }
    u32 previous = 0, current = shape[0] | (scale ? scale[0] : 0);
    for (u32 y = 0; y < BADGE_SIZE; ++y) {
        u32 next = y + 1 < BADGE_SIZE ? shape[y + 1] | (scale ? scale[y + 1] : 0) : 0;
        u32 white = shape[y] | (active ? active[y] : 0);
        u32 near = previous | current | next;
        /* The native symbols have a one-pixel black outline. Do not touch
           transparent pixels, or the rest of the live-view canvas. */
        u32 bits = near | (near << 1) | (near >> 1);
        while (bits) {
            u32 x = (u32)__builtin_ctz(bits), bit = 1U << x;
            u32 color = white & bit ? 0xffffffffU : current & bit ? 0xff808080U : 0xff000000U;
            SURFACE_PIXEL(s, BADGE_LEFT + x, BADGE_TOP + y) = color;
            bits &= bits - 1;
        }
        previous = current;
        current = next;
    }
}
