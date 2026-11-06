# AF1 – an F1 pit wall

A three-screen live Formula 1 dashboard on ESP32-S3 boards (LCDwiki ES3C35P, 3.5" 480x320), mounted on
a 3D-printed pit-wall stand. Left: standings. Middle: live track map. Right: race drama and sound.

Status: **Phase 1 (build guide Jobs 3–5)**. Shared library and a replay test sketch. Compiles; not yet
run on the boards. See [docs/build-guide.md](docs/build-guide.md) for the full plan.

```
AF1/                    shared Arduino library, used by every board sketch
  src/AF1.h             everything a sketch can call
  src/AF1.cpp           screen (ST77922 init), touch (tap/hold/swipe), wifi + NZ clock
  src/F1Data.cpp        OpenF1 replay: session, drivers, lap count, running order
  src/secrets.example.h copy to secrets.h (git-ignored) with the home wifi
CheckpointA/            test sketch: NZ clock + replayed race lap count
test/                   host unit tests (fakes for Arduino/WiFi/HTTP), 100% line + branch gate
tools/probe_f1_live.sh  shows which F1 live-timing topics are free right now
docs/                   build guide, hardware notes, data sources, assets, history
```

## Set up (once)

1. `cp AF1/src/secrets.example.h AF1/src/secrets.h` and fill in the 2.4 GHz wifi.
2. Link the library into the sketchbook: `ln -s "$PWD/AF1" ~/Arduino/libraries/AF1`
3. Libraries: GFX Library for Arduino 1.6.9, ArduinoJson 7.4.x. Board settings are in
   [docs/hardware.md](docs/hardware.md#arduino-settings-all-boards).

## Checkpoint A

Open `CheckpointA/CheckpointA.ino`, set `BOARD_NAME`, and upload. You should see:

- the NZ time and date;
- "Loading race data..." for about 10 s;
- the latest finished race (e.g. "Mexico City - Race");
- "On the grid", then "Lap 1 / 71" ticking at real speed with the top 3.

Tap = replay speed 1x / 10x / 60x. Hold = restart. Serial Monitor (115200) logs every OpenF1 request.

## Tests

```
make -C test
```

Builds the library and the sketch on your PC against small fakes in `test/fakes/`, then runs 183 checks.
It fails unless every line and every branch ran (C++ exception paths aren't counted). Needs g++ and
python3; ArduinoJson comes from `~/Arduino/libraries` (set `ARDUINOJSON=` to change that). CI runs
the tests and an ESP32 compile on every push.

## Data

Testing uses OpenF1's free history. For live races, F1's own timing feed is free except for car
positions on track. See [docs/data-sources.md](docs/data-sources.md) for the options the maker needs to pick from.

## Not in yet (added with the job that needs it)

- `settings` (Preferences) – Job 10. `link` (ESP-NOW; switch wifi to AP_STA) – Job 15.
- Tyres, intervals, pits, race control, weather – Jobs 8, 12, 17.
- Live data source – Job 24.

Code: [MIT licence](LICENSE). Credits: [CREDITS.md](CREDITS.md). Personal, non-commercial project, not affiliated with Formula 1.
