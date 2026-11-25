# Light Leak website

This branch contains the standalone site for the
[unofficial Light Leak module](https://github.com/brunolimawd/grkit-light-leak).
The module, its releases and hardware validation records are maintained separately
from the site. Serve the checked-in HTML as-is, without a build, backend or asset
CDN. A small local script enhances the comparison and menu; the guide and
installation instructions also work without JavaScript.

## Publish on GitHub Pages

1. Push this branch: `git push -u origin gh-pages`.
2. In the module repository, open **Settings → Pages**. Under **Build and
   deployment**, choose **Deploy from a branch**, then **`gh-pages`** and
   **`/(root)`**. The `.nojekyll` file keeps the site static.
3. Check [the site](https://brunolimawd.github.io/grkit-light-leak/) after deploy,
   including the translated routes and a missing route that should return 404.
4. Verify the site as a URL-prefix property in Google Search Console and Bing
   Webmaster Tools and submit [`sitemap.xml`](https://brunolimawd.github.io/grkit-light-leak/sitemap.xml).

Do not add a `robots.txt` here expecting it to control crawlers: GitHub Pages
serves this project below `/grkit-light-leak/`, while robots rules are read at the
host root. If the canonical origin changes, update the metadata, sitemap and
404 links together.

For local preview, run `python3 -m http.server 8000` in the branch root.
The three page pairs work at `http://localhost:8000/`. To also exercise the
production path (including links from `404.html`), serve a parent directory
where this checkout is named or symlinked as `grkit-light-leak`.

## Update the site with a release

- Make edits to the shared header, footer and home pages in `tools/sync-site.py`,
  then run `python3 tools/sync-site.py` and commit the updated HTML. The
  installation and guide contents can be edited directly in their HTML files.
  `python3 tools/sync-site.py --check` checks whether the generated pages match.
- Check the module's README and release notes for the tested body, firmware,
  runtime ABI and behavior before editing the EN and PT-BR pages.
- Update the beta label and release state in `install/index.html` and
  `pt-br/install/index.html`. Only offer a direct download after the exact
  tagged GitHub release has a public `LITLEAK.GRM` asset. Do not upload the
  module binary to this branch or use a generic `/releases/latest` link for
  a prerelease.
- Keep the locale switch and canonical/alternate links reciprocal. Add new
  indexable URLs to the sitemap. Keep FAQ answers in the HTML and link to
  sources for compatibility and recovery.

## Analytics

The six content pages load Google Tag Manager container `GTM-52MK6S86`.
Analytics tags and their trigger and consent settings are managed in Google
Tag Manager. This repository does not configure a separate Google Analytics
measurement ID.

## Sample photo and credits

The single demonstration photograph is by
[MIKI Yoshihito](https://www.flickr.com/photos/mujitra/53311001084/),
licensed under [CC BY 2.0](https://creativecommons.org/licenses/by/2.0/).
The input is [GRKit's base development](https://grkit.app/landing/looks/standard.jpg).
The gallery assets are resized and re-encoded; the “after” versions and social
preview are modified by the actual Light Leak filter in GRKit's host test
harness. The hero uses Medium strength, Original position and Variation 1.
The photo was taken on a GR IIIx; the off-camera processing does not claim
hardware validation of the module on that body.
The checked-in examples were rendered from
[module commit `e459ca1`](https://github.com/brunolimawd/grkit-light-leak/commit/e459ca12c5ed64195460951db3ed802901a765e3).
The decoded pixels are converted to a full-range YCbCr 4:2:0 plane and
back for both sides of the comparison:

```sh
# Run from this branch. Point `module` to a clean module checkout; run
# grkit module build in that checkout to generate the SDK staging directory.
module=/path/to/grkit-light-leak
objects="$module/build/LITLEAK.objects"
sdk="$objects/runtime/sdk"
work=$(mktemp -d)
npm install --prefix "$work" --no-audit --no-fund sharp@0.35.5
curl -fLS https://grkit.app/landing/looks/standard.jpg -o "$work/standard.jpg"
clang -std=c11 -O2 -I "$sdk" -I "$objects" -I "$module" \
  -include grkit_test.h tools/render-sample.c "$sdk/grkit_test.c" \
  "$module/light_leak.c" "$module/reference.c" "$module/slit.c" \
  -o "$work/render-sample"
node tools/render-samples.mjs "$work/node_modules/sharp" "$work/render-sample" "$work/standard.jpg"
```

Layout and component styles are adapted from
[GRKit](https://github.com/ahonn/GRKit/tree/61dd9a697668d809939c2a75d402d76e418ff6fc/web)
by ahonn and contributors (MIT). The module tiles are from the module branch;
the ADJ tile frame is derived from GRKit under the MIT license. IBM Plex Sans,
Sans Condensed and Mono are distributed under the
[SIL Open Font License](assets/IBM-Plex-OFL.txt). Site code is under the
[MIT License](LICENSE); the sample photo and its derivatives retain CC BY 2.0.
