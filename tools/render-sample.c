/* Host-only preview tool using the module and GRKit's unmodified test harness.
   Input and output are full-range Y followed by interleaved 4:2:0 CbCr. */
#include "grkit_test.h"
#include "grkit_module.h"
#include "exposure.h"
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 1200U
#define HEIGHT 800U
#define LUMA_BYTES (WIDTH * HEIGHT)
#define PLANE_BYTES (LUMA_BYTES + LUMA_BYTES / 2)
_Static_assert(PLANE_BYTES <= GRKIT_TEST_PLANE_BYTES, "sample fits in host plane");

void light_leak_apply(PictureSet *set, u32 value);

static int setting(const char *text, unsigned long min, unsigned long max, u32 *out)
{
    char *end;
    unsigned long value = strtoul(text, &end, 10);
    if (end == text || *end || value < min || value > max) return 0;
    *out = (u32)value;
    return 1;
}

int main(int argc, char **argv)
{
    u32 style, strength, position, variation;
    if (argc != 5 ||
        !setting(argv[1], LOOK_SLIT, LOOK_FOG, &style) ||
        !setting(argv[2], 0, 2, &strength) ||
        !setting(argv[3], ENTRY_RIGHT, ENTRY_ORIGINAL, &position) || position == ENTRY_RANDOM ||
        !setting(argv[4], VARIANT_FIRST, VARIANT_THIRD, &variation)) return 2;
    u8 *bytes = (u8 *)grkit_test_bytes(GRKIT_TEST_PLANES);
    if (fread(bytes, 1, PLANE_BYTES, stdin) != PLANE_BYTES) return 1;

    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    set.planes[0] = (Plane){GRKIT_TEST_PLANES, GRKIT_TEST_PLANES + LUMA_BYTES,
        WIDTH, HEIGHT, WIDTH, 1, 0, 0, WIDTH, HEIGHT};
    setting_set(SETTING_STRENGTH, strength);
    setting_set(SETTING_POSITION, position);
    setting_set(SETTING_VARIATION, variation);
    light_leak_apply(&set, style);

    return fwrite(bytes, 1, PLANE_BYTES, stdout) == PLANE_BYTES ? 0 : 1;
}
