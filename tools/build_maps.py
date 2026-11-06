#!/usr/bin/env python3
"""Fit OpenF1 car coordinates onto the 23 season maps and write the map board's map files.

Usage (from the repo root):
  python3 tools/build_maps.py            fit all 23 rounds and write the files
  python3 tools/build_maps.py 6 11       re-fit rounds 6 and 11 only (the rest keep their saved fits)
  python3 tools/build_maps.py --no-fit   just rewrite the files from the saved fits (tools/.cache/fits.json)
Needs: numpy, pillow (pip install numpy pillow). Internet on the first run only.

For each round it downloads one clean lap of one car from the free OpenF1 API (cached in
tools/.cache/), finds the transform that lays that lap onto the drawn track line, and writes:
  MapBoard/src/mapNN.cpp          the map as two PNGs (left/right halves) in PROGMEM byte arrays
  MapBoard/src/maps.h             table: circuit key, round, name, PNGs, transform
  tools/.cache/overlay_NN.png     the lap drawn in cyan on the map - check these by eye
  tools/.cache/contact_sheet.png  all 23 overlays on one page
"""
import calendar, io, json, math, os, re, sys, time, urllib.request
import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, "tools", ".cache")
OUT = os.path.join(ROOT, "MapBoard", "src")
API = "https://api.openf1.org/v1/"

# round: (OpenF1 circuit_key, name) - 2026 calendar order
ROUNDS = {
    1: (10, "Albert Park Circuit"), 2: (49, "Shanghai International Circuit"),
    3: (46, "Suzuka Circuit"), 4: (151, "Miami International Autodrome"),
    5: (23, "Circuit Gilles Villeneuve"), 6: (22, "Circuit De Monaco"),
    7: (15, "Circuit De Barcelona-Catalunya"), 8: (19, "Red Bull Ring"),
    9: (2, "Silverstone Circuit"), 10: (7, "Spa-Francorchamps"), 11: (4, "Hungaroring"),
    12: (55, "Circuit Zandvoort"), 13: (39, "Autodromo Nazionale Monza"), 14: (153, "Madring"),
    15: (144, "Baku City Circuit"), 16: (12, "Sepang International Circuit"),
    17: (61, "Marina Bay Street Circuit"), 18: (9, "Circuit Of The Americas"),
    19: (65, "Autodromo Hermanos Rodriguez"), 20: (14, "Interlagos"),
    21: (152, "Las Vegas Strip Circuit"), 22: (150, "Lusail International Circuit"),
    23: (70, "Yas Marina Circuit"),
}

_last = 0.0
def get(query):
    """GET an OpenF1 endpoint, cached on disk, at most one request every 2 s."""
    global _last
    path = os.path.join(CACHE, re.sub(r"[^A-Za-z0-9_.=-]", "_", query) + ".json")
    if os.path.exists(path):
        return json.load(open(path))
    time.sleep(max(0, _last + 2.0 - time.time()))
    _last = time.time()
    print("  GET", query)
    try:
        with urllib.request.urlopen(API + query, timeout=60) as r:
            data = json.load(r)
    except urllib.error.HTTPError as e:
        if e.code != 404:  # OpenF1 says 404 when nothing matches
            raise
        data = []
    json.dump(data, open(path, "w"))
    return data

def one_lap(circuit_key):
    """(x, y) arrays of one normal racing lap at this circuit."""
    races = [s for s in get(f"sessions?circuit_key={circuit_key}&session_name=Race")
             if s["year"] in (2024, 2025, 2026) and s["date_end"] < time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime())]
    races.sort(key=lambda s: s["date_start"], reverse=True)
    for s in races:
        laps = [l for l in get(f"laps?session_key={s['session_key']}")
                if l.get("lap_duration") and l.get("date_start") and l["lap_number"] >= 3 and not l.get("is_pit_out_lap")]
        if not laps:
            continue
        med = sorted(l["lap_duration"] for l in laps)[len(laps) // 2]
        for lap in sorted(laps, key=lambda l: abs(l["lap_duration"] - med))[:3]:  # a few tries: location can have gaps
            t0 = lap["date_start"][:19]
            t1 = time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime(
                calendar.timegm(time.strptime(t0, "%Y-%m-%dT%H:%M:%S")) + lap["lap_duration"] + 1))
            loc = get(f"location?session_key={s['session_key']}&driver_number={lap['driver_number']}"
                      f"&date>{t0}&date<{t1}")
            pts = np.array([(p["x"], p["y"]) for p in loc if p["x"] or p["y"]], float)
            if len(np.unique(pts, axis=0)) > 150:  # some laps have only a few distinct samples
                print(f"  {s['year']} session {s['session_key']} car {lap['driver_number']} lap {lap['lap_number']}: {len(pts)} points")
                return pts
    sys.exit(f"no usable lap for circuit {circuit_key}")

def densify(pts, step):
    """Closed polyline resampled every `step` units (so coverage isn't biased by sample gaps)."""
    loop = np.vstack([pts, pts[:1]])
    out = []
    for a, b in zip(loop[:-1], loop[1:]):
        n = max(1, int(np.hypot(*(b - a)) / step))
        out.append(a + (b - a) * np.arange(n)[:, None] / n)
    return np.vstack(out)

def track_pixels(img):
    """Pixel (x, y) of the speed-coloured track line: saturated, bright, hue red..green."""
    a = np.asarray(img.convert("RGB"), float) / 255
    mx, mn = a.max(2), a.min(2)
    sat = (mx - mn) / np.maximum(mx, 1e-6)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    mask = (sat > 0.55) & (mx > 0.5) & (b < 0.6 * mx) & (r + g > 0.8)  # red/orange/yellow/green, not blue/purple
    mask[:38, :] = False    # title, round badge, flag
    mask[292:, :] = False   # legend
    ys, xs = np.nonzero(mask)
    return np.column_stack([xs, ys]).astype(float)

def nearest(a, b):
    """For each point in a, distance to the nearest point in b."""
    d = np.full(len(a), np.inf)
    for i in range(0, len(b), 400):
        c = b[i:i + 400]
        d = np.minimum(d, np.sqrt(((a[:, None, :] - c[None, :, :]) ** 2).sum(2)).min(1))
    return d

def apply(p, lap):
    """p = (ax, bx, cx, ay, by, cy) -> pixel coordinates."""
    ax, bx, cx, ay, by, cy = p
    return np.column_stack([ax * lap[:, 0] + bx * lap[:, 1] + cx, ay * lap[:, 0] + by * lap[:, 1] + cy])

def distance_map(track, limit=25):
    """Approximate distance (px) from every pixel to the track line, by repeated dilation, capped at limit."""
    d = np.full((320, 480), float(limit))
    d[track[:, 1].astype(int), track[:, 0].astype(int)] = 0
    for _ in range(limit):
        p = np.pad(d, 1, constant_values=limit)
        d = np.minimum.reduce([d] + [p[1 + dy:321 + dy, 1 + dx:481 + dx] + math.hypot(dx, dy)
                                     for dy in (-1, 0, 1) for dx in (-1, 0, 1) if dx or dy])
    return d

def score(p, lap, target, limit=25):
    """Lap-to-track distance plus track-to-lap distance (so a tiny fit on one straight loses)."""
    dmap, sub = target
    q = apply(p, lap)
    x, y = np.round(q[:, 0]).astype(int), np.round(q[:, 1]).astype(int)
    inside = (x >= 0) & (x < 480) & (y >= 0) & (y < 320)
    on_track = np.where(inside, dmap[np.clip(y, 0, 319), np.clip(x, 0, 479)], limit).mean()
    return on_track + np.minimum(nearest(sub, q[::4]), limit).mean()

def params(rot, mirror, sx, sy, tx, ty, shear=0.0):
    """Rotation/mirror/scale/shift -> affine. Image y points down, so mirror=1 is the 'normal' flip."""
    c, s = math.cos(rot), math.sin(rot)
    m = -1 if mirror else 1
    # rotate, then (optionally) mirror y, then scale x/y, then shear
    ax, bx = sx * c, -sx * s
    ay, by = sy * m * s, sy * m * c
    ax, bx = ax + shear * ay, bx + shear * by
    return (ax, bx, tx, ay, by, ty)

def refine(v, lapc, target):
    """Pattern search: nudge each parameter both ways, halve the steps when nothing helps."""
    sc = score(params(*v), lapc, target)
    steps = [math.radians(1.5), 0, v[2] * 0.03, v[3] * 0.03, 4.0, 4.0, 0.02]
    for _ in range(10):
        improved = True
        while improved:
            improved = False
            for i, st in enumerate(steps):
                for sign in (1, -1):
                    w = list(v)
                    w[i] += sign * st
                    s2 = score(params(*w), lapc, target)
                    if s2 < sc - 1e-4:
                        v, sc, improved = w, s2, True
        steps = [s / 2 for s in steps]
    return v, sc

def fit(lap, track):
    """Coarse search over rotation x mirror, then local refinement. Returns (params, score)."""
    lapc = lap - lap.mean(0)
    target = (distance_map(track), track[::max(1, len(track) // 1500)])
    tmin, tmax = track.min(0), track.max(0)
    coarse = []
    for mirror in (0, 1):
        for deg in range(0, 360, 3):
            r = math.radians(deg)
            p = params(r, mirror, 1, 1, 0, 0)
            q = apply(p, lapc)
            w, h = q.max(0) - q.min(0)
            s = min((tmax[0] - tmin[0]) / w, (tmax[1] - tmin[1]) / h)
            p = params(r, mirror, s, s, 0, 0)
            q = apply(p, lapc)
            off = (tmin + tmax) / 2 - (q.min(0) + q.max(0)) / 2
            v = [r, mirror, s, s, off[0], off[1], 0.0]
            coarse.append((score(params(*v), lapc, target), v))
    coarse.sort(key=lambda c: c[0])
    starts = []  # the 4 best starts at least 10 degrees apart: the very best coarse one can be a trap
    for _, v in coarse:
        if len(starts) < 4 and all(c[1] != v[1] or abs(c[0] - v[0]) > math.radians(10) for c in starts):
            starts.append(v)
    best = None
    for v in starts:
        v, sc = refine(v, lapc, target)
        if best is None or sc < best[1]:
            best = (v, sc)
    v, sc = best
    ax, bx, cx, ay, by, cy = params(*v)
    mx, my = lap.mean(0)  # fold the centring back in so the transform takes raw OpenF1 x, y
    return (ax, bx, cx - ax * mx - bx * my, ay, by, cy - ay * mx - by * my), sc

def write_cpp(rnd, name):
    """mapNN.cpp: the map as two PNGs, left and right halves (240 px wide each). PNGdec 1.1.6 can only
    buffer lines up to about 420 RGB pixels, so a 480-wide truecolour PNG overflows it."""
    img = Image.open(os.path.join(ROOT, "assets", "maps", f"map{rnd:02d}.png")).convert("RGB")
    sizes = []
    with open(os.path.join(OUT, f"map{rnd:02d}.cpp"), "w") as f:
        f.write(f"// Round {rnd} - {name} (made by tools/build_maps.py)\n#include <Arduino.h>\n")
        for half, x in (("left", 0), ("right", 240)):
            buf = io.BytesIO()
            img.crop((x, 0, x + 240, 320)).save(buf, "PNG", optimize=True)
            data = buf.getvalue()
            sizes.append(len(data))
            f.write(f"extern const uint8_t map{rnd:02d}_{half}[] PROGMEM;\n"
                    f"const uint8_t map{rnd:02d}_{half}[] PROGMEM = {{\n")
            for i in range(0, len(data), 24):
                f.write(",".join(str(b) for b in data[i:i + 24]) + ",\n")
            f.write("};\n")
    return sizes

def main():
    os.makedirs(CACHE, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    args = sys.argv[1:]
    fit_rounds = [] if args == ["--no-fit"] else [int(a) for a in args] or list(ROUNDS)
    fits_path = os.path.join(CACHE, "fits.json")
    fits = json.load(open(fits_path)) if os.path.exists(fits_path) else {}
    for rnd in fit_rounds:
        key, name = ROUNDS[rnd]
        print(f"Round {rnd}: {name}")
        img = Image.open(os.path.join(ROOT, "assets", "maps", f"map{rnd:02d}.png")).convert("RGB")
        lap = one_lap(key)
        p, sc = fit(densify(lap, np.ptp(lap, 0).max() / 400), track_pixels(img))
        print(f"  score {sc:.2f}")
        fits[str(rnd)] = p
        json.dump(fits, open(fits_path, "w"))
        q = apply(p, densify(lap, 20))
        ImageDraw.Draw(img).line([tuple(t) for t in q] + [tuple(q[0])], fill=(0, 255, 255), width=2)
        img.save(os.path.join(CACHE, f"overlay_{rnd:02d}.png"))

    # mapNN.cpp and maps.h for every round, from the saved fits
    lines = ["// The 23 season maps in round order, with the transform from OpenF1 track x/y to screen",
             "// pixels: px = ax*x + bx*y + cx, py = ay*x + by*y + cy. Made by tools/build_maps.py.",
             "// Each map is two PNGs, the left and right 240 px of the screen.",
             "#pragma once", "#include <Arduino.h>", "",
             "struct MapInfo", "{", "  int circuitKey, round;", "  const char *name;",
             "  const uint8_t *left, *right;", "  uint32_t leftSize, rightSize;",
             "  float ax, bx, cx, ay, by, cy;", "};", ""]
    lines += [f"extern const uint8_t map{r:02d}_left[], map{r:02d}_right[];" for r in ROUNDS]
    lines += ["", "static const MapInfo MAPS[] = {"]
    for r, (key, name) in ROUNDS.items():
        left, right = write_cpp(r, name)
        p = fits.get(str(r), [0] * 6)
        lines.append(f'    {{{key}, {r}, "{name}", map{r:02d}_left, map{r:02d}_right, {left}, {right}, '
                     + ", ".join(f"{v:#.7g}f" for v in p) + "},")
    lines += ["};", "static const int MAP_COUNT = sizeof(MAPS) / sizeof(MAPS[0]);", ""]
    open(os.path.join(OUT, "maps.h"), "w").write("\n".join(lines))

    sheet = Image.new("RGB", (4 * 480, 6 * 320))
    for r in ROUNDS:
        f = os.path.join(CACHE, f"overlay_{r:02d}.png")
        if os.path.exists(f):
            sheet.paste(Image.open(f), ((r - 1) % 4 * 480, (r - 1) // 4 * 320))
    sheet.save(os.path.join(CACHE, "contact_sheet.png"))

if __name__ == "__main__":
    main()
