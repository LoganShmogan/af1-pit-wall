# Standings Board (left screen)

Shows the running order of the race replay, synced with the other two boards.

## What it shows

- **Header:** lap count ("LAP 12 / 71", or "GRID" before lights out), circuit and session, NZ time.
- **List (default):** every driver in order, P1-11 on the left and P12-22 on the right. Each row has
  the position, a team-colour bar, the 3-letter code, the gap to the leader ("LEADER", "+12.3",
  "+1 LAP") and the tyre (coloured ring: S red, M yellow, H white, I green, W blue) with laps on that set.
- **Top-3 feature:** every 90 s the list gives way to P1, P2 and P3, about 6 s each, full screen:
  position on a team-colour panel (the photo goes there later), code, gap, tyre and best lap.
- **Driver card:** code in team colour, "Started P5 - now P2 (+3)", gap, tyre, best and last lap, pit stops.
- While the race loads: "Loading race..." with the latest problem underneath, if there is one.

## Touch

- Tap a driver row: open their card. Tap anywhere (or wait 8 s) to go back.
- Tap during the top-3 feature: back to the list.

## Upload settings

Same as Checkpoint A (see `docs/hardware.md`): Board **ESP32S3 Dev Module**, USB CDC On Boot
**Enabled**, PSRAM **OPI PSRAM**, Flash Size **16MB**, Partition Scheme **Default**.
Needs the AF1 library and `AF1/src/secrets.h` with the wifi details.

`SESSION_KEY` at the top of the sketch picks the race (0 = latest finished). Keep `REPLAY_SPEED` at 1
so all three boards show the same moment.
