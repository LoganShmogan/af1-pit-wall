# Data sources – what's free, what needs F1 TV (checked 7 Nov 2026)

Short version: **build and test on OpenF1's free history (works today, no account).** For live races,
F1's own timing hub is free for everything **except car x/y positions**. Exact positions need either
an F1 TV Access login or a paid OpenF1 account.

## What I tested

| Source | Login? | Result on 7 Nov 2026 |
| --- | --- | --- |
| OpenF1 REST, finished sessions (`api.openf1.org/v1/…`) | No | ✅ Works. Mexico 2026 race: 1,360 laps, 557 position rows, `location` x/y available after the session |
| OpenF1 during a live session | Paid account | Not tested. Free data appears about 30 min after a session ends |
| F1 archive `livetiming.formula1.com/static/<year>/…` | **F1 TV** | ❌ 401 for every year (2023–2026) |
| F1 archive root `static/StreamingStatus.json`, `SessionInfo.json` | No | ✅ |
| F1 live hub `livetiming.formula1.com/signalrcore` | No | ✅ Connects. Snapshot returns the topics below |

Live hub topics without a login (snapshot after Friday's São Paulo session, re-check with
`tools/probe_f1_live.sh` during a live session):

| Topic | Free? | Gives us |
| --- | --- | --- |
| `SessionInfo` | ✅ | meeting, session type, circuit |
| `TimingData` | ✅ | running order, gaps, intervals, sector and lap times |
| `TimingAppData` | ✅ | tyre stints (compound, new/used, laps) |
| `DriverList` | ✅ | codes, names, team colours |
| `RaceControlMessages` | ✅ | flags, safety car, incidents |
| `TrackStatus` | ✅ | green / yellow / SC / VSC / red |
| `WeatherData` | ✅ | air and track temperature, rain |
| `LapCount` | ? | missing in the snapshot (it was practice; races only) |
| `Position.z` | ❌ | car x/y on track – **this is the map board** |
| `CarData.z` | ❌ | speed, throttle, gear |

## What changed in October 2026

Around 15 Oct 2026, F1 put the whole past-session archive behind an F1 TV login: a CloudFront rule
answers 401 for anything under `/static/<year>/`. FastF1 3.9.0 (released 24 Oct) and undercut-f1
4.0.105 added F1 TV sign-in because of this. What's known from those projects:

- The token is the F1 TV `subscriptionToken`, sent as `Authorization: Bearer <token>`.
- It lasts about **4 days** and can't be refreshed automatically, so someone has to sign in again.
- It needs **F1 TV Access** or higher (about NZ$41.99/year).
- The live feed is unofficial, and F1 can change it at any time (it just did).

Sources: [FastF1 #978](https://github.com/theOehrly/Fast-F1/issues/978),
[undercut-f1 #252](https://github.com/JustAman62/undercut-f1/issues/252),
[FastF1 auth docs](https://docs.fastf1.dev/versions/dev/api_reference/accounts_auth.html),
[olamva/f1 #384](https://github.com/olamva/f1/pull/384), [OpenF1 auth](https://openf1.org/auth.html).
I couldn't confirm OpenF1's Sponsor price from an official page (a third-party listing says EUR 9.90; the
OpenF1 request form says usage-based).

## How the boards fetch (as built)

- **Once per race**, in the background: session, drivers, then the feeds a board asked for (laps, positions,
  tyres, pits, race control, weather). Loads in about 16–25 s on the boards.
- **Streamed** just ahead of the replay clock: gaps (`intervals`, 60 s windows) and car positions (`location`,
  15 s windows, map board only).
- **Request budget** at replay speed 1: about 7 requests a minute for all three boards together, well under the
  free tier's ~30. At higher replay speeds the map's car positions can fall behind and skip ahead.
- **Gzip:** every reply is requested compressed (OpenF1 only does this over HTTP/1.1). It matters because the
  ESP32's link to OpenF1 is about 15 KB/s from NZ: a 15 s window of car positions is 161 KB plain, 5.5 KB gzip.

## What this means for the build

1. **Now (Jobs 5–23):** OpenF1 free history, replayed (`AF1/src/F1Data.cpp`). Includes `location` x/y, so
   the map board can be built and calibrated fully. No account needed.
2. **Live, free option (new Option D):** the F1 hub without login for timing, tyres, flags and weather.
   The map would place cars by **estimating** position along the track from lap and sector timing,
   not from real x/y. Cost: nothing. The catch: the dots are approximate between timing points.
3. **Live, exact positions:** either
   - **A. OpenF1 paid account**: same code as the replay, add a token (refreshed hourly with a username and password). Easiest.
   - **B. F1 TV Access + helper board**: the token is pasted in about every 4 days. Most work, and the most fragile.

Recommendation: keep building on (1). The maker decides between D, A and B before Job 24. A and D need the
least code: A only changes the URL and token; D adds a second parser that fills the same `F1Race` struct.
