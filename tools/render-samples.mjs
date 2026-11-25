import { spawnSync } from 'node:child_process';
import { createRequire } from 'node:module';
import { resolve } from 'node:path';

const [, , sharpPath, rendererPath] = process.argv;
if (!sharpPath || !rendererPath) {
  throw new Error('Usage: node tools/render-samples.mjs /path/to/node_modules/sharp /path/to/render-sample');
}
const sharp = createRequire(import.meta.url)(resolve(sharpPath));
const renderer = resolve(rendererPath);
const width = 1200;
const height = 800;
const pixels = width * height;
const base = await sharp('assets/coastal-line.svg').flatten().toColourspace('srgb').raw().toBuffer();
if (base.length !== pixels * 3) throw new Error('Unexpected illustration size');
const clamp = (value) => Math.min(255, Math.max(0, Math.round(value)));
const yuv = Buffer.alloc(pixels * 1.5);

for (let y = 0; y < height; y += 2) {
  for (let x = 0; x < width; x += 2) {
    let cb = 0;
    let cr = 0;
    const i = y * width + x;
    for (const j of [i, i + 1, i + width, i + width + 1]) {
      const r = base[j * 3];
      const g = base[j * 3 + 1];
      const b = base[j * 3 + 2];
      const luma = 0.299 * r + 0.587 * g + 0.114 * b;
      yuv[j] = clamp(luma);
      cb += (b - luma) / 1.772;
      cr += (r - luma) / 1.402;
    }
    const c = pixels + (y / 2) * width + x;
    yuv[c] = clamp(128 + cb / 4);
    yuv[c + 1] = clamp(128 + cr / 4);
  }
}

const rgb = (planes) => {
  const raw = Buffer.alloc(pixels * 3);
  for (let y = 0, i = 0; y < height; y++) {
    for (let x = 0; x < width; x++, i++) {
      const c = pixels + (y >> 1) * width + (x & ~1);
      const luma = planes[i];
      const cb = planes[c] - 128;
      const cr = planes[c + 1] - 128;
      raw[i * 3] = clamp(luma + 1.402 * cr);
      raw[i * 3 + 1] = clamp(luma - 0.344136 * cb - 0.714136 * cr);
      raw[i * 3 + 2] = clamp(luma + 1.772 * cb);
    }
  }
  return sharp(raw, { raw: { width, height, channels: 3 } });
};

const exportImage = async (image, name, size) => {
  await image.clone().resize(size).webp({ quality: 87 }).toFile(`assets/${name}.webp`);
};

const styles = ['slit-flare', 'burnt-edge', 'film-band', 'amber-veil', 'prismatic-arc', 'color-fog'];
await exportImage(rgb(yuv), 'coastal-original', 1200);
await exportImage(rgb(yuv), 'coastal-original-small', 600);
for (const [index, style] of styles.entries()) {
  const result = spawnSync(renderer, [String(index + 2)], { input: yuv, maxBuffer: yuv.length * 2 });
  if (result.error || result.status !== 0 || result.stdout?.length !== yuv.length || result.stdout.equals(yuv)) {
    throw new Error(`Could not render ${style}`, { cause: result.error });
  }
  const image = rgb(result.stdout);
  await exportImage(image, style, 600);
  if (index === 0) {
    await exportImage(image, 'slit-flare-large', 1200);
    await image.clone().resize(1200, 630).jpeg({ quality: 88, mozjpeg: true }).toFile('assets/share.jpg');
  }
}
