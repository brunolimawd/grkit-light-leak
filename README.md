# Light Leak website

This branch contains the standalone site for the
[unofficial Light Leak module](https://github.com/brunolimawd/grkit-light-leak).
The module, its releases and hardware validation records are maintained separately
from the site. No build, JavaScript runtime or external asset CDN is required to
serve these pages.

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

## Illustrations

`assets/coastal-line.svg` is original artwork for the site, not a stock photo or
a camera sample. The after images are the actual module filter run in GRKit's
host test harness, using Medium strength, Original position and Variation 1.
The checked-in examples were rendered from
[module commit `e459ca1`](https://github.com/brunolimawd/grkit-light-leak/commit/e459ca12c5ed64195460951db3ed802901a765e3).
The raw illustrated pixels are converted to a full-range YCbCr 4:2:0 plane and
back for both sides of the comparison:

```sh
# Run from this branch. Point `module` to a clean module checkout; run
# grkit module build in that checkout to generate the SDK staging directory.
module=/path/to/grkit-light-leak
objects="$module/build/LITLEAK.objects"
sdk="$objects/runtime/sdk"
work=$(mktemp -d)
npm install --prefix "$work" --no-audit --no-fund sharp@0.35.5
clang -std=c11 -O2 -I "$sdk" -I "$objects" -I "$module" \
  -include grkit_test.h tools/render-sample.c "$sdk/grkit_test.c" \
  "$module/light_leak.c" "$module/reference.c" "$module/slit.c" \
  -o "$work/render-sample"
node tools/render-samples.mjs "$work/node_modules/sharp" "$work/render-sample"
```

The site design takes cues from [GRKit.app](https://grkit.app/), without using
its logo or photos. IBM Plex Sans Condensed is distributed under the
[SIL Open Font License](assets/IBM-Plex-OFL.txt). Site code and original artwork
are under the [MIT License](LICENSE).
