# Map board (middle screen)

The live track map: every car in its team colour on the signed-off season map.

## What it shows

- **During a race** (at a circuit with a map): the circuit map, and on it
  - each car as a team-colour dot with its number, plus a small tyre dot (S red, M yellow, H white, I green, W blue);
  - podium rings: P1 gold always, P2 silver and P3 blue while within 2 s of the car ahead, pulsing faster as the gap closes;
  - battle rings: the 4 closest fights (under 1 s) behind the podium, flashing in team colours;
  - red ripples on a crashed car for 10 s;
  - safety car: yellow frame and a strobing "SC" / "VSC" badge top left; red flag: red banner;
  - along the bottom: running order (two rows), lap count and the NZ time.
- **Tap a car**: its code and position for 3 s.
- **No race** (or a circuit without a map): the 23 season maps. Tap the right half or swipe left for
  the next one, tap the left half or swipe right to go back.

## Upload

Same board settings as the other sketches ([docs/hardware.md](../docs/hardware.md)), plus
**Tools > Partition Scheme = Custom**. That uses `partitions.csv` in this folder, which gives the
program 12 MB so the 23 maps fit. Libraries: AF1, GFX Library for Arduino, PNGdec.

## Rebuilding the maps

The maps live in `assets/maps/map01.png` ... `map23.png` (480x320, round order). After changing one:

```
python3 tools/build_maps.py            # fit all 23 rounds (about 30 s each) and write the files
python3 tools/build_maps.py 6          # re-fit round 6 only
python3 tools/build_maps.py --no-fit   # just rewrite the files from the saved fits
```

It needs numpy and pillow. It downloads one lap of real car positions per circuit from OpenF1
(cached in `tools/.cache/`), fits it onto the drawn track line, and rewrites `src/mapNN.cpp` and
`src/maps.h` (the calibration). Check `tools/.cache/overlay_NN.png`: the cyan lap must sit on the track.
The current fits for all 23 maps are in [docs/map-fits.jpg](../docs/map-fits.jpg).

Each map is stored as two PNGs, the left and right halves of the screen: PNGdec 1.1.6 can only buffer
lines up to about 420 colour pixels, so a full 480-wide PNG fails to decode part-way down.
