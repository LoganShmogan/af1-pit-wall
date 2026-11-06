# Drama Board (right screen)

Shows what is happening in the race replay, synced with the other two boards. Calm most of the time;
big moments take over the screen.

## What it shows

- **Top bar:** lap count ("LAP 12 / 71", or "GRID" before lights out), a track status pill (GREEN,
  VSC, SC, RED, CHEQUERED), NZ time and a speaker icon (crossed out when muted).
- **Left:** the fight for the lead (P1 vs P2 with the gap), fastest lap (driver, time, lap), fastest
  pit stop (driver, time), weather (air and track temperature, humidity, "RAIN").
- **Right:** news feed, newest on top: the last 6 items with lap number, a colour marker per kind and
  the short text.
- **Cards:** a crash, red flag, safety car or VSC takes over the screen for 8 s with the title
  ("CRASH - TURN 4", "RED FLAG", ...), the driver code(s) on team-colour panels and the full text.
  A lead change or fastest lap gets a banner across the middle for 4 s. One at a time; the rest wait,
  most important first (crash > red flag > SC/VSC > lead > fastest lap).
- **Sound (placeholder):** no audio yet. For each big moment the board prints the commentary clip
  it would play to Serial, e.g. `SOUND: crash at turn 04.mp3`. Never two within 3 s; pit stops and
  penalties stay quiet. `clipFor()` holds the clip table.
- While the race loads: "Loading race..." with the latest problem underneath, if there is one.

## Touch

- Tap a news item: open its card again. Tap a card: close it.
- Tap the speaker (top right): mute / unmute.
- Hold the top bar: toy button, a test crash at turn 4 for the leader.

## Upload settings

Same as Checkpoint A (see `docs/hardware.md`): Board **ESP32S3 Dev Module**, USB CDC On Boot
**Enabled**, PSRAM **OPI PSRAM**, Flash Size **16MB**, Partition Scheme **Default**.
Needs the AF1 library and `AF1/src/secrets.h` with the wifi details.

`SESSION_KEY` at the top of the sketch picks the race (0 = latest finished). Keep `REPLAY_SPEED` at 1
so all three boards show the same moment.

## Tests

`make -C test drama` (100% line and branch coverage of the sketch).
