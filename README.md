# Light Leak for GRKit

Light Leak is an **unofficial third-party module** for
[GRKit](https://github.com/ahonn/grkit). It simulates light entering the
edge of a film camera. It is installed separately from the official
modules and does not modify camera firmware.

## Compatibility

| Camera | Firmware | Hardware validation |
| --- | --- | --- |
| RICOH GR III | 2.10 | Tested |

Other models and firmware versions have not been verified on hardware
for this module. The camera must already have a GRKit runtime compatible
with **module ABI 2.7**. The reference build uses GRKit 1.6.6 at the
revision in [`GRKIT_REV`](GRKIT_REV).

## Install or update

1. Download `LITLEAK.GRM` from [Releases](https://github.com/brunolimawd/grkit-light-leak/releases).
2. With the camera off, copy it to `GRKIT/MODULES/LITLEAK.GRM` at the
   root of the SD card. Replace that file when upgrading; do not copy
   it to `SCRIPT/GRKIT/MODULES`.
3. Turn the camera on and wait for **GRK** to appear in ADJ. If Light Leak
   is missing, check `GRKIT/REPORT.TXT`. GRKit needs one free slot for
   the external module.

Remove the `.GRM` file to uninstall. If the camera stops before the menu
opens, follow the [GRKit module recovery instructions](https://github.com/ahonn/grkit/blob/61dd9a697668d809939c2a75d402d76e418ff6fc/docs/modules.md#if-the-camera-stops-at-power-on).

## Use

In **ADJ → GRK → Light Leak**, choose a style:

| Reference style | Original entry | 1 · Original | 2 | 3 |
| --- | --- | --- | --- | --- |
| Slit Flare | Right | White beam, red curtain and cyan reflection | Shorter cut with a shallow gold edge | Wide diagonal beam with a detached blue trail |
| Burnt Edge | Right | Irregular bright seam and red-orange spill | Localized pale corner burn | Deep copper burn with a plum halo |
| Film Band | Bottom | Raised gold band with a red lip and cool fringe | Two red flashes close to the edge | Deeper, slanted gold and cyan band |
| Amber Veil | Top | Amber pool with a diagonal red tail | Compact, honey-colored pool | Long, rust-colored sweep in the opposite direction |
| Prismatic Arc | Right | Red, gold, teal and blue arc | Tight blue-green crescent close to the edge | Wide gold arc with a separate blue-violet echo |
| Color Fog | Top | Diagonal gold, teal, blue and rose wash | Cool sea-green and violet | Warm rose and gold on the opposite slant |

Use **Fn → Strength** for Weak, Medium, or Strong. **Fn → Leak Position**
offers Right, Left, Top, Bottom, Random, and Original for all six reference
styles. Original uses the entry in the table. **Fn → Variation** offers
**1** (the original look), **2** and **3** (different light shapes and
palettes within the same style), or **Auto** (the default). Every style
has all three variations on every side. Auto chooses one per shot
without changing the style, strength or side.

The **Random** style independently draws a reference style, strength and
side, without changing the saved Strength or Leak Position. It also
draws a variation if Variation is set to Auto, or uses the fixed
number otherwise. All draws are shared by the JPEG and its previews, and
consecutive shots can repeat. Each draw uses
the capture metadata and light samples from the main JPEG plane, not a
firmware clock call or mutable state carried between encodes.

The standalone v16 release uses the same module ID and settings as the
v16 integration build. Existing settings are kept on upgrade from v11
or later. If upgrading from a prototype before v11, select the style
again in ADJ.

The six reference styles calculate the light at the native sample centers
of each JPEG plane, without enlarging a raster mask. The source photos
served only as visual references; no pixels, scratches, or watermarks
from them are included. Clear areas do not darken the image, and clipped
highlights retain their original values. The directions are tied to the
sensor and rotate with portrait shots. Monochrome receives the same
brightness lift without tint. The defaults are Off, Medium, Original
position and Auto variation.

The effect is written into the JPEG and its previews. It does not modify
the raw sensor data, has no live-view preview, and is not stored in Recipes.
It runs on the visible image after the color look and Glow, before Film-like
Grain and Date Imprint. It does not color the added border.

## Viewfinder icon

With **Show Icon** on, the left of the still viewfinder shows the active
style as a white symbol when DISP shows full information. One, two, or
three white segments show Weak, Medium, or Strong; the remaining
segments are gray. Random shows a die without segments because its
strength is drawn at capture. Off shows nothing.

**Show Icon** defaults to **On** and is saved. If an earlier installation
saved Off, use **Fn → Show Icon → On** to enable it. The ADJ icons are
unchanged; the viewfinder symbol is never part of the saved image.

The badge uses a live-view layer so it can coexist with all six registered
GRKit status icons. While the badge is visible, GRKit hides the camera's
native grid, as it does for other live-view layers. Turn Show Icon off to
restore the previous grid behavior, or use the GRKit Guides module.

## Build from source

Use Rust 1.92 or later and ARM-capable Clang and LLD. Install the pinned
CLI from a clean checkout of this repository, then build the module:

```sh
cargo install --locked --git https://github.com/ahonn/grkit.git \
  --rev "$(cat GRKIT_REV)" grkit-cli
grkit module test --dir .
mkdir -p build
grkit module build --dir . --out build/LITLEAK.GRM
```

Use `--clang` and `--lld` if the tools are not on your `PATH`. The build
does not need Ricoh firmware or a separate GRKit checkout: the CLI
supplies the SDK. Use a fresh output directory for rebuilds.

See [CONTRIBUTING.md](CONTRIBUTING.md) for checks and icon regeneration,
and [CHANGELOG.md](CHANGELOG.md) for release notes. Code and artwork are
under the [MIT License](LICENSE); see [NOTICE.md](NOTICE.md) for credits.
