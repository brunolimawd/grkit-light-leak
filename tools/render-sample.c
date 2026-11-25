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

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    char *end;
    unsigned long style = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end || style < LOOK_SLIT || style > LOOK_FOG) return 2;
    u8 *bytes = (u8 *)grkit_test_bytes(GRKIT_TEST_PLANES);
    if (fread(bytes, 1, PLANE_BYTES, stdin) != PLANE_BYTES) return 1;

    PictureSet set = {.count = 1, .scratch = grkit_test_scratch};
    set.planes[0] = (Plane){GRKIT_TEST_PLANES, GRKIT_TEST_PLANES + LUMA_BYTES,
        WIDTH, HEIGHT, WIDTH, 1, 0, 0, WIDTH, HEIGHT};
    setting_set(SETTING_STRENGTH, 1);
    setting_set(SETTING_POSITION, ENTRY_ORIGINAL);
    setting_set(SETTING_VARIATION, VARIANT_FIRST);
    light_leak_apply(&set, (u32)style);

    return fwrite(bytes, 1, PLANE_BYTES, stdout) == PLANE_BYTES ? 0 : 1;
}
