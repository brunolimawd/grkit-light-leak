//! Render Light Leak's native-size viewfinder masks into `badge_art.h`.
//! This standalone generator does not modify the ADJ source PNGs.
//! Wedge and line geometry adapted from GRKit's MIT-licensed module-icons
//! example; see NOTICE.md.
use std::{fs, io, path::PathBuf};

const BADGE_SIZE: usize = 32;
const SAMPLES: usize = 16;
type Point = (f64, f64);

#[derive(Clone, Copy)]
enum Shape {
    Random,
    Slit,
    Burnt,
    Band,
    Veil,
    Prism,
    Fog,
}

fn segment_distance((x, y): Point, a: Point, b: Point) -> f64 {
    let (dx, dy) = (b.0 - a.0, b.1 - a.1);
    let t = (((x - a.0) * dx + (y - a.1) * dy) / (dx * dx + dy * dy)).clamp(0.0, 1.0);
    (x - a.0 - t * dx).hypot(y - a.1 - t * dy)
}

// The same strength wedge as the GRKit status icons, at Light Leak's size.
fn wedge(level: usize, x: f64, y: f64) -> bool {
    const SCALE: f64 = 40.0 / BADGE_SIZE as f64;
    const LEFT: f64 = 2.0;
    const RIGHT: f64 = 37.0;
    const BOTTOM: f64 = 37.0;
    const GAP: f64 = 3.0;
    let segment = (RIGHT - LEFT - GAP * 2.0) / 3.0;
    let offset = x * SCALE - LEFT;
    if !(0.0..RIGHT - LEFT).contains(&offset) {
        return false;
    }
    let index = (offset / (segment + GAP)) as usize;
    let inside = offset - index as f64 * (segment + GAP) < segment;
    let height = 3.0 + 7.0 * offset / (RIGHT - LEFT);
    let y = y * SCALE;
    index < level && inside && y >= BOTTOM - height && y < BOTTOM
}

fn badge_shape(shape: Shape, (x, y): Point) -> bool {
    let frame = (4.0..28.0).contains(&x) && (2.0..19.0).contains(&y);
    let frame_edge = frame && (!(5.6..26.4).contains(&x) || !(3.6..17.4).contains(&y));
    match shape {
        Shape::Random => {
            let dx = (x - 16.0).abs();
            let dy = (y - 16.0).abs();
            let edge = (dx - 6.8).max(0.0).hypot((dy - 6.8).max(0.0));
            (1.6..=3.5).contains(&edge)
                || [
                    (0.0, 0.0),
                    (-4.5, -4.5),
                    (4.5, -4.5),
                    (-4.5, 4.5),
                    (4.5, 4.5),
                ]
                .iter()
                .any(|(cx, cy)| (x - 16.0 - cx).hypot(y - 16.0 - cy) <= 1.55)
        }
        Shape::Slit => {
            let dx = (x - 25.0).abs();
            let dy = (y - 7.5).abs();
            segment_distance((x, y), (3.6, 15.7), (24.5, 7.7)) <= 0.95
                || segment_distance((x, y), (10.1, 17.1), (19.1, 13.7)) <= 0.65
                || (dx <= 4.7 && dy <= 0.9)
                || (dx <= 0.9 && dy <= 4.6)
                || dx + dy <= 2.5
        }
        Shape::Burnt => {
            let seam = [22.4, 19.0, 22.7, 20.1, 23.6, 20.8, 22.2];
            if !frame {
                return false;
            }
            let t = (y - 2.0) * 6.0 / 17.0;
            let i = t.floor() as usize;
            let bend = seam[i] + (seam[i + 1] - seam[i]) * t.fract();
            frame_edge || x >= bend
        }
        Shape::Band => frame_edge || (7.4..24.6).contains(&x) && (11.0..14.5).contains(&y),
        Shape::Veil => {
            let t = (x - 4.5) / 23.0;
            if !(0.0..1.0).contains(&t) {
                return false;
            }
            let end = ((t - 0.76).max(0.0) / 0.24).powi(2);
            let width = (0.8 + 4.9 * t) * (1.0 - end).sqrt();
            let axis = 17.2 - 15.0 * t + 4.0 * t * t;
            (y - axis).abs() <= width
        }
        Shape::Prism => {
            let dx = x - 25.5;
            let dy = y - 10.5;
            let r = dx.hypot(1.85 * dy);
            dx <= -2.0 && dy.abs() < 8.5 && ((r - 16.2).abs() <= 1.2 || (r - 10.7).abs() <= 1.2)
        }
        Shape::Fog => [
            (8.0, 27.0, 5.3, 0.0),
            (4.5, 28.1, 10.6, 0.45),
            (6.5, 24.0, 16.1, 0.9),
        ]
        .into_iter()
        .any(|(left, right, center, phase)| {
            if !(left..right).contains(&x) {
                return false;
            }
            let arch = ((x - 6.0) * 0.29 + phase).sin() * 1.38;
            let taper = ((x - left).min(right - x) / 3.0).min(1.0);
            (y - center - arch).abs() <= 1.13 * taper
        }),
    }
}

fn badge_rows(inside: impl Fn(f64, f64) -> bool) -> [u32; BADGE_SIZE] {
    let mut rows = [0; BADGE_SIZE];
    // Leave room for a one-pixel outline. Use binary ink like the native icons.
    for (y, row) in rows.iter_mut().enumerate().skip(1).take(BADGE_SIZE - 2) {
        for x in 1..BADGE_SIZE - 1 {
            let mut count = 0;
            for sy in 0..SAMPLES {
                for sx in 0..SAMPLES {
                    count += usize::from(inside(
                        x as f64 + (sx as f64 + 0.5) / SAMPLES as f64,
                        y as f64 + (sy as f64 + 0.5) / SAMPLES as f64,
                    ));
                }
            }
            if count * 2 >= SAMPLES * SAMPLES {
                *row |= 1 << x;
            }
        }
    }
    rows
}

fn append_rows(header: &mut String, rows: [u32; BADGE_SIZE], name: &str) {
    header.push_str(&format!("    {{ /* {name} */\n"));
    for chunk in rows.chunks_exact(4) {
        header.push_str("        ");
        for bits in chunk {
            header.push_str(&format!("0x{bits:08x}U, "));
        }
        header.pop();
        header.push('\n');
    }
    header.push_str("    },\n");
}

fn main() -> io::Result<()> {
    let path = std::env::args_os()
        .nth(1)
        .map(PathBuf::from)
        .unwrap_or_else(|| "badge_art.h".into());
    let mut header = format!(
        "/* Generated by tools/generate-badge.rs; bit 0 is the left pixel. */\n\
         #define BADGE_SIZE {BADGE_SIZE}U\n\n\
         static const u32 badge_symbols[][BADGE_SIZE] = {{\n"
    );
    // Same order as LOOK, starting at Random. Off does not paint.
    for (name, shape) in [
        ("Random", Shape::Random),
        ("Slit Flare", Shape::Slit),
        ("Burnt Edge", Shape::Burnt),
        ("Film Band", Shape::Band),
        ("Amber Veil", Shape::Veil),
        ("Prismatic Arc", Shape::Prism),
        ("Color Fog", Shape::Fog),
    ] {
        append_rows(
            &mut header,
            badge_rows(|x, y| badge_shape(shape, (x, y))),
            name,
        );
    }
    header.push_str("};\n\nstatic const u32 badge_steps[][BADGE_SIZE] = {\n");
    for (level, name) in [(1, "Weak"), (2, "Medium"), (3, "Strong")] {
        append_rows(&mut header, badge_rows(|x, y| wedge(level, x, y)), name);
    }
    header.push_str("};\n");
    if !fs::read_to_string(&path).is_ok_and(|current| current == header) {
        fs::write(path, header)?;
    }
    Ok(())
}
