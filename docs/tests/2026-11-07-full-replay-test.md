# Full replay test – 7 Nov 2026

The first time all three boards run a whole race together on the real hardware, from the grid to the
chequered flag, with nobody touching them.

**Status:** running. The findings section is filled in afterwards.

## What's being tested

- All three boards start the race at the same moment and stay in step for the whole race (same lap,
  leader and events at the same time), without talking to each other.
- The map board: cars move smoothly and stay on the drawn track, rings and markers appear at the right time.
- The standings board: order, gaps and tyres follow the race; the top-3 feature comes and goes.
- The drama board: lap, track status, the fight for the lead, fastest lap and pit stop, weather, news feed,
  incident cards, and the commentary clip it picks for each moment (logged only; sound is Job 16).
- Running on mains power, with no computer attached, for about 2.5 hours.

## Setup

| Item | Value |
| --- | --- |
| Race | Mexico City GP 2026, OpenF1 session 11404 (latest finished race), replayed at real speed |
| Start | **1:55 PM NZDT** (`2026-11-07T00:55:00` UTC): every board holds the grid until then, then races from the session start |
| Boards | Left: StandingsBoard. Middle: MapBoard. Right: DramaBoard. LCDwiki ES3C35P each |
| Code | Commit after `5289454`, plus the start-time setting and the lower wifi transmit power (committed with this write-up) |
| Power | Each board on its own USB power after flashing |
| Wifi | Home 2.4 GHz network |

### How the boards were flashed

The start time is built in with a compiler flag, so normal uploads (and the Arduino IDE) keep the usual
looping replay. From the repo root, for each board in turn (MapBoard adds `,PartitionScheme=custom` to the FQBN):

```
arduino-cli compile --upload -p /dev/ttyACM0 \
  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi,FlashSize=16M \
  --library AF1 \
  --build-property 'compiler.cpp.extra_flags=-DF1_REPLAY_START="2026-11-07T00:55:00"' \
  DramaBoard
```

To repeat the test, pick a new start time about 20 minutes ahead (UTC) and flash all three with it.
A board powered on after the start time joins at the right moment, in step with the others.

### What each board loaded (serial log, 115200)

| Board | Loaded | Time to load |
| --- | --- | --- |
| Drama | 22 drivers, 71 laps, 3704 events | 20 s |
| Map | 22 drivers, 71 laps, 3441 events | 19 s |
| Standings | 22 drivers, 71 laps, 3272 events | 16 s |

Each then held the grid and started streaming gaps (and, on the map, car positions) from 20:00:00 race time.

## Problems found while setting up

| Problem | Cause | Fix |
| --- | --- | --- |
| Map board "Sketch too big" in the IDE | Default partition only has 1.3 MB for the program; the map board is 3.9 MB | Partition Scheme = Custom, Flash Size = 16MB (now in the README) |
| Only 2 of 71 laps loaded | The ESP32's TLS client returns "no data" while the next packet is on its way | Whole replies collected with `writeToStream()` (commit `5d3877b`) |
| Car positions fell behind the replay | About 15 KB/s link to OpenF1 from NZ | Gzip-compressed replies, 15–30x smaller (commit `5d3877b`) |
| Drama board never joined the wifi (router: "auth expired", reason 2) | That board's radio at full transmit power; a router restart only helped once | Transmit power lowered to 8.5 dBm on all boards; it joins in under a second |
| Black screen, silent serial after reading the board's MAC | `esptool` left the board in download mode | Watchdog reset (`esptool --after watchdog-reset chip-id`) or the RST button |

## What should happen (NZ time)

Race time maps onto the clock as 20:00:00 race = 1:55 PM. Times are to the minute.

| NZ time | Lap | Event | Expect to see |
| --- | --- | --- | --- |
| 1:55 PM | – | Replay starts | All boards leave the grid hold together; "GRID" / lap 0 |
| 1:58 PM | 1 | Lights out | Lap 1; drama "LIGHTS OUT" card; cars move on the map |
| 1:58 PM | 1 | LEC takes the lead from HAM | Drama lead banner; P1 changes on standings and map |
| 2:02 PM | 4 | Turn 6 collision, RUS | Map: red ripple at RUS; drama: crash card ("CRASH - TURN 6") |
| 2:03 PM | – | First pit stops begin (58 in the race) | Drama news + fastest pit; standings tyres change |
| 2:07 PM | 7 | Virtual safety car | Map: VSC badge + yellow frame; drama: VSC card, status VSC |
| 2:09 PM | 8 | VSC ending | Back to green |
| 2:15–2:16 PM | 12–13 | Penalties: LEC 5 s, RUS 10 s | Drama news |
| 2:36–2:57 PM | – | Lead changes during the stops: HAM, ANT, then VER | Lead banners; P1 rings swap |
| 2:59 PM | 44 | Safety car | Map: SC badge; drama: SC card, status SC |
| 3:01 PM | 45 | **Red flag** | Red banner (map), RED status and card (drama); cars stop in the pit lane |
| 3:01–3:40 PM | 45–46 | Stopped | Nothing moves for about 40 min. Expected, not a fault |
| 3:34 PM | 46 | Penalty: PER 5 s | Drama news |
| 3:40 PM | 46 | Standing restart | Back to green, cars race again |
| 3:46 PM | 49 | Turn 4 collision, HUL | Map ripple at HUL; drama crash card |
| 3:49 PM | 51 | Penalty: HUL 10 s | Drama news |
| 4:00 PM | 59 | Safety car | SC badge and card |
| 4:06 PM | 62 | Safety car in this lap | Back to green |
| 4:18 PM | 71 | Chequered flag | Drama "CHEQUERED FLAG" card; status CHEQUERED |
| 4:25 PM | – | Replay loops to the grid | All boards back to lap 0 together, then the race again |

## Findings

*(Filled in after the test.)*

| Area | What to check | Result | Notes |
| --- | --- | --- | --- |
| Sync | Same lap and leader on all three at the same time | | |
| Sync | Events land together across boards (lights out, crash, SC, red flag) | | |
| Map | Cars move smoothly, stay on the track line | | |
| Map | Podium and battle rings readable, not cluttered | | |
| Map | Crash ripples at the right car; SC/VSC badge; red banner | | |
| Standings | Order, gaps and tyres follow the race | | |
| Standings | Top-3 feature every 90 s, back to the list | | |
| Drama | Lap, status, lead fight, fastest lap/pit, weather correct | | |
| Drama | Cards appear at the right moments and don't pile up | | |
| All | Readable from a normal viewing distance | | |
| All | Ran the whole race without freezing, restarting or losing wifi | | |
| All | Back to the grid together when the replay loops | | |

**Other observations:**

-
