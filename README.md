# AF1 – an F1 pit wall

A three-screen live Formula 1 dashboard on ESP32-S3 boards (LCDwiki ES3C35P, 3.5" 480x320), mounted on
a 3D-printed pit-wall stand. Left: standings. Middle: live track map. Right: race drama and sound.

Status: **all three boards, version 1, running on the real hardware** (7 Nov 2026) on a replay of the latest
finished race, in step with each other. Progress against the plan is ticked off in
[docs/build-guide.md](docs/build-guide.md#11-build-order-and-checklist).

## One folder per board

Each board has its own sketch folder. Open the folder's `.ino` in the Arduino IDE and upload it to that board.

| Folder | Board | Shows |
| --- | --- | --- |
| [StandingsBoard/](StandingsBoard/) | left | running order, gaps, tyres, top-3 feature, driver cards |
| [MapBoard/](MapBoard/) | middle | the circuit map with every car moving, podium/battle rings, crash and safety car markers |
| [DramaBoard/](DramaBoard/) | right | lap, track status, the fight for the lead, fastest lap/pit, weather, news feed, incident cards |
| [CheckpointA/](CheckpointA/) | any | test sketch: NZ clock + replayed lap count |

Each folder's README has what it shows, the touch controls and its upload settings. All three boards
read the same race through the shared `AF1` library. Each works out the replay moment from the
internet clock, so they show the same moment without talking to each other.

```
AF1/                    shared Arduino library, used by every board
  src/AF1.h             everything a sketch can call
  src/AF1.cpp           screen (ST77922 init), touch (tap/hold/swipe), wifi + NZ clock
  src/F1Data.cpp        OpenF1 replay: laps, order, tyres, gaps, pits, race control, weather, car x/y, news
  src/secrets.example.h copy to secrets.h (git-ignored) with the home wifi
assets/maps/            the 23 signed-off season maps (PNG)
tools/build_maps.py     turns the maps into MapBoard/src and fits each one to OpenF1's car positions
tools/probe_f1_live.sh  shows which F1 live-timing topics are free right now
test/                   host unit tests (fakes for Arduino/WiFi/HTTP), 100% line + branch gate
docs/                   build guide, hardware notes, data sources, assets, history
```

## Set up (once)

1. `cp AF1/src/secrets.example.h AF1/src/secrets.h` and fill in the 2.4 GHz wifi.
2. Link the library into the sketchbook: `ln -s "$PWD/AF1" ~/Arduino/libraries/AF1`
3. Libraries (Library Manager): GFX Library for Arduino 1.6.9, ArduinoJson 7.4.x, PNGdec 1.1.6.

## Uploading a board

Open the board's folder (e.g. `MapBoard/MapBoard.ino`), plug that board in, then in **Tools**:

| Setting | Choose |
| --- | --- |
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| PSRAM | OPI PSRAM |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | Default; **Custom** for the map board |
| Port | /dev/ttyACM0 (or ttyACM1) |

Upload. The board joins the wifi, loads the race in about 20 s and starts the replay.
To start all three from the grid at the same moment (a test run), see
[the full replay test](docs/tests/2026-11-07-full-replay-test.md#how-the-boards-were-flashed). Serial Monitor
(115200) shows each step. If something goes wrong, see
[troubleshooting](docs/hardware.md#flashing-and-troubleshooting).

To update a board later, change only its folder and upload that one sketch. To change something all
three boards share (data, wifi, touch), edit `AF1/` and upload all three.

## Tests

```
make -C test            # everything
make -C test map        # or: lib, standings, drama
```

Builds the library and each board on your PC against small fakes in `test/fakes/` and runs the checks
(library 342, map 87, standings 46, drama 82).
It fails unless every line and every branch ran (C++ exception paths aren't counted). Needs g++ and
python3; ArduinoJson and PNGdec come from `~/Arduino/libraries` (set `ARDUINOJSON=` / `PNGDEC=` to
change that). CI runs the tests and compiles every sketch for the ESP32 on every push.

## Data

Testing uses OpenF1's free history. For live races, F1's own timing feed is free except for car
positions on track. See [docs/data-sources.md](docs/data-sources.md) for the options the maker needs to pick from.
The free tier allows about 30 requests a minute for the whole house; at replay speed 1, the three
boards together use about 7. Replies are fetched gzip-compressed because the boards' link to OpenF1 is slow.

## Not in yet

- Sound playback on the drama board (Job 16). It already picks the right clip and logs it.
- Board-to-board link (ESP-NOW): pinning from the standings board, the synced idle show (Jobs 15, 21).
- Map layers menu, pin halos and follow-zoom (Jobs 10, 11); driver photos (Job 13).
- Practice and qualifying views (Job 22); live data source (Job 24).

Code: [MIT licence](LICENSE). Credits: [CREDITS.md](CREDITS.md). Personal, non-commercial project, not affiliated with Formula 1.
