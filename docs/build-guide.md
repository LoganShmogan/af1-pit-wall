# AF1 Dashboard – Build Guide

Nov 7, 2026

## 1. Project overview

AF1 is a three-screen live Formula 1 dashboard that sits on a 3D-printed McLaren-style pit wall, mounted on the wall at home. It is a gift for the user (a McLaren / Lando Norris fan), so it must look like a piece of art in an expensive house, not a hobby gadget.

**The three screens, left to right (all landscape):**

| Position | Screen | Job during a session | Speaker |
| --- | --- | --- | --- |
| Left | Standings board | Full running order, tyres, tyre age, lap count, driver cards, pinning | Optional |
| Middle (hero) | Map board | Live cars on the track map, battle rings, safety car, crashes, follow-zoom | Optional |
| Right | Drama board | Events, incident cards, pit stops, weather, fastest lap, Ryan's commentary, all sound | Yes (main) |

Between race weekends all three screens run one shared attract/idle show.

**Who does what**

- **Maker:** owns the look and feel, signs off every screen before it is called done.
- **Developer:** owns the code, uploads, data conversion and testing on the real boards.
- **Claude:** can help the developer with code, map/image conversion and research when asked.

**Hard rules**

- No paid APIs, except the optional F1 TV Access subscription (about NZ$41.99/year) for exact car positions.
- No official F1 logo. Use the AF1 badge.
- No copied broadcast audio or commentary. Real recorded sound effects only, no synth sounds (one exception: the start-lights sequence).
- Everything is saved in the maker's Documents > F1 Dashboard folder.

## 2. What it's made of

The screens are three LCDwiki ES3C35P boards from Temu (not the Cheap Yellow Display first planned). One extra ESP32-S3 board acts as the helper.

| Part | Qty | Details | Status |
| --- | --- | --- | --- |
| LCDwiki ES3C35P display board | 3 | ESP32-S3 N16R8 (16 MB flash, 8 MB OPI PSRAM), 3.5 in 320x480 IPS, ST77922 driver on QSPI, capacitive touch, native USB-C | Have |
| Spare ESP32-S3 board (two USB-C ports) | 1 | Helper board: F1 TV sign-in and position relay (see section 5) | Have |
| Small speaker (came in the box) | 1-3 | 40 x 27 x 10 mm, 50 mm cable (35-40 mm usable). The developer to check: real speaker or buzzer? | Have 1 |
| MAX98357 I2S amplifier | 1 | Only needed if the in-box speaker can't be driven directly | Check |
| 4 ohm 3 W speaker | 1 | Backup if the in-box one is a buzzer | Check |
| 5 V 4 A multi-port USB wall charger | 1 | Must have overcurrent protection | To buy |
| 1-to-4 USB-C splitter cable | 1 | One charger port, four USB-C plugs (3 screens + helper) | To buy (Temu) |
| Right-angle USB-C cables | 3-4 | Cable 2 mm thick; plugs enter the board's side edge | To buy |
| White LED strip | 1 | Single colour, hard-wired (not coded), under the canopy | To buy |
| 3D-printed pit-wall stand | 1 | Printed on the maker's Bambu P2S; McLaren colours or single colour | Design v14 done |
| M3 screws | 12 | Board mounting holes are 3 mm | To buy |

**Board measurements (for the stand):**

- Mounting holes: 94.5 mm apart along the long side, 48 mm across the short side, M3.
- Screen glass: 83 x 54 mm.
- Depth: 10 mm from glass to tallest plug, 13 mm with the speaker plug and wires.
- Straight USB-C plug: 26 mm long, 11 mm wide, 5.5-6 mm thick (right-angle cables will reduce this).

**Pit-wall stand (v14, signed off by the maker):** copies McLaren's real pit wall. Mounted high on a wall, back flat and flush. Three screens in a straight row, tops tilted forward to face down at the viewer. Front panel reads "<user's name>'s Pit Wall" plus a small McLaren logo (gift only, not for sale). Two headsets on hooks under the desk, a scale keyboard on the desk, small helmet on top. A speaker pocket above each screen with slotted grille, rear cooling vents, snap-in cable clips, and wiring holes for the canopy LED strip. Canopy ends closed so the strip can't be seen from the side.

## 3. What connects where

Each of the three screens is its own ESP32-S3 with its own program; they all fetch the same live data, so during a session they stay in sync off the race clock.

&#91;embedded content: system diagram · data, board links, sound and power\]

**Connections in words:**

- **Data:** every board joins the home WiFi and pulls race data itself (OpenF1, or the helper board if F1 TV is used).
- **Helper board (optional):** signs in to F1 TV; the sign-in code is pasted from the maker's phone into a simple web page the helper shows. It then passes exact car positions to the three boards.
- **Board to board:** ESP-NOW. The map board is the conductor for the idle show ("flip to next now"). Pins set on the standings board are sent to the map board so the car lights up.
- **Sound:** speaker (plus MAX98357 I2S amp if needed) on the drama board. Speaker pockets exist above all three screens in case more are used later.
- **Power:** one 5 V 4 A charger, one 1-to-4 USB-C splitter, right-angle plugs into each board's side edge.
- **Canopy LED strip:** plain white, wired straight to power, not controlled by code.

## 4. Dev setup and memory budget

The toolchain is already proven: the developer has run Job 1 (text on screen) and Job 2 (touch test) on the real boards, and the Albert Park map has displayed correctly.

**Arduino settings (all three boards):**

| Setting | Value | Note |
| --- | --- | --- |
| IDE | Arduino IDE 1.8.19 | Works; IDE 2 not needed |
| Board | ESP32S3 Dev Module |  |
| USB CDC On Boot | Enabled | Critical: black screen and silent serial if Disabled |
| PSRAM | OPI PSRAM |  |
| Port (Linux Mint) | /dev/ttyACM0 | Native USB |

**Screen driver:** the ST77922 is not supported by Arduino\_GFX or other common libraries. Use LCDwiki's own init sequence and QSPI code from their Example\_01\_Simple\_test demo (29 demos are in the maker's Downloads > F1 RACING folder). Pins: QSPI data 9/11/12/13/14, CS 10, backlight 41. Wrap this once in a shared `display` module and reuse it on all three boards.

**Suggested shared code (one library, three sketches):**

- `display` - LCDwiki init, drawing, image blit from flash.
- `touch` - capacitive touch reads, tap and hold events.
- `net` - WiFi connect, reconnect, NTP time (NZ time zone).
- `f1data` - OpenF1 polling and parsing into one shared race-state struct.
- `link` - ESP-NOW messages between boards.
- `settings` - saves layer toggles, pins, volume across power-off (NVS/Preferences).

**File formats:**

- Maps and images: byte-swapped RGB565 header files (proven working on the board). One 480x320 map is about 300 KB.
- Audio: MP3, never WAV, to keep it small.

**Memory budget (16 MB flash per board):** 23 maps + program about 8 MB; all audio as MP3 about 1.5 MB (commentary about 0.57 MB, effects about 0.8 MB). Total under 10 MB, about 6 MB spare. Show one map at a time; 8 MB PSRAM is free for frame buffers and the zoom view.

## 5. Data sources

**Decision needed first:** OpenF1 is free only for historical data. Live data during a session needs a logged-in OpenF1 account ([OpenF1 auth guide](https://openf1.org/auth.html)), which is a paid Sponsor tier. A third-party listing puts it at about EUR 9.90 and says the free data opens up from 30 minutes after a session ends ([listing](https://freeapihub.com/apis/openf1-api)). Check openf1.org for the current price and billing period. This affects the "no paid APIs" rule and the earlier "free estimated positions" plan.

**Three ways to feed live data:**

| Option | Cost | Exact car positions | Live? | Notes |
| --- | --- | --- | --- | --- |
| A. OpenF1 Sponsor account | Small subscription | Yes (`location` x/y/z) | Yes, via REST, MQTT or WebSocket | Easiest to code; token expires hourly, refresh it |
| B. F1 TV Access + helper board | About NZ$41.99/year | Yes | Yes | Helper signs in to F1 live timing and relays to the 3 screens; most work |
| C. Free OpenF1 only | Free | Yes, but only after the session | No (delayed replay) | Good for building and testing every feature now |

**Recommendation for the developer:** build and test everything on Option C (replay a past race from historical data as if live). Write the data layer so the source can be swapped, then the maker picks A or B before going live.

**OpenF1 endpoints used:**

| Endpoint | Used for | Board |
| --- | --- | --- |
| `sessions`, `meetings` | Which circuit, session type, start times | All |
| `location` | Car x/y on track | Map |
| `position` | Running order | Standings, Map |
| `intervals` | Gaps to leader / car ahead, battle detection | All |
| `laps` | Lap number, sector times, hot-lap detection | All |
| `stints` | Tyre compound and stint start lap | Standings |
| `pit` | Pit stop times | Drama |
| `race_control` | Flags, safety car, incidents and their corner | Drama, Map |
| `weather` | Air/track temp, rain | Drama |
| `drivers` | Names, numbers, team colours, headshot\_url | All |
| `championship` / points source | Points table for idle mode and standings flip | Standings |

**Known data quirks:**

- `tyre_age_at_start` is often wrong (reports 0 on used tyres). Compute tyre age = current lap - stint lap\_start + 1. The compound itself is reliable.
- Free tier rate limit is about 3 requests/second and 30/minute. Three boards polling separately can hit this; if so, let one board fetch and share over ESP-NOW.
- No live fuel data exists anywhere; fuel was ruled out.
- Sector split points are not in free data; maps use even-thirds approximations.
- `location` x/y uses F1's own coordinate system; it must be scaled and rotated to fit each saved map image (one calibration per circuit).

## 6. Use cases

Each use case is something a person does or sees; every one should be testable on the real boards.

| # | Who | Situation | What should happen |
| --- | --- | --- | --- |
| U1 | Anyone | No session this week | All 3 screens run the synced idle show (section 10) |
| U2 | User | Race starts | Screens go live on their own, right track loads, start-lights sound plays, Ryan says "lights out" |
| U3 | User | Watching a race | Sees every car move on the map, the running order with tyres, and events on the drama board |
| U4 | User | Wants to follow Lando | Taps him (map or standings list), picks a halo colour, his car flashes on the map |
| U5 | User | Wants a close-up | Taps "Follow 30 seconds" or "Stay on"; map zooms and tracks the user's car; "Unfollow" button returns to full track |
| U6 | Maker | Also following a driver | Pins their own car with a different colour (up to 4 pinned in total, first come first served) |
| U7 | Anyone | A crash happens | Red ripple on the map, crash sound, Ryan calls "crash at turn N", drama board shows an incident card |
| U8 | Anyone | Safety car out | Yellow beacon marker on map, Ryan calls it, drama board shows reason and laps out on tap |
| U9 | Anyone | Close battle | Team-colour rings flash on the two cars, faster as the gap shrinks; drama board zooms in on it for about 30 seconds |
| U10 | Anyone | Pit stop | Rattle-gun sound; drama board shows stop time and rank |
| U11 | Guest | Walks up curious | Notices the small menu icon, taps, discovers layers, driver cards, toys |
| U12 | Anyone | Wants a quieter screen | Map layers toggled off to a clean white outline; settings remembered after power-off |
| U13 | Anyone | Too loud | Volume slider or one-tap mute on screen |
| U14 | Anyone | Qualifying | Hot-lap car pulses, sectors light purple/green/yellow on the middle map, close-up on a side screen, timing order on the other |
| U15 | Anyone | Practice | Calm view: session name, times, who's quickest |
| U16 | User | Secret | Hidden tap on the AF1 badge shows a personal message from the maker |
| U17 | Anyone | Power cut or WiFi drop | Boards reconnect on their own and resume, no laptop needed |

## 7. Map board (middle, build first)

The map board is the hero screen and the ESP-NOW "conductor". Keep it clean: the cars racing on the track, plus time, date, location and lap count.

**Base map**

- 23 season maps (v4, signed off), landscape, converted to byte-swapped RGB565.
- On race weekends it auto-loads the current circuit from `sessions`. Otherwise the maker can scroll through all 23 in calendar order.
- Each map shows: round badge (top corner), country flag (other corner), track name (top middle), speed-gradient track line (green fast to red slow), numbered corners, purple/green/yellow sector line on the inside, streets or contours faintly behind, time and date bottom right, speed key along the bottom.

**Layers (tap to toggle, remembered after power-off):** background streets/contours, speed colours, sector lines, corner numbers. Hidden behind a small always-visible menu or gear icon; tapping it slides the toggles in and out.

**Car markers**

- Car icon (not a dot) in team colour with driver number; small tyre-compound icon beside it.
- Thin strip at the screen edge with the numbered running order.
- Podium rings: P1 gold, P2 silver, P3 electric blue. Silver/blue switch on when the gap is under 2 s and fade over 3 s. Rings pulse out of phase and swap cars on a pass. Pulse speeds up as the gap closes (about 1.5 s apart slow, under 0.5 s fast).
- Battle rings: any two cars fighting for a position flash in team colours, faster when closer. Show only the 4-5 fiercest battles at once.
- Optional small 3-letter code (e.g. NOR) by the top 3. No faces on this screen.
- Overlap (start, safety car, tight chicanes): nudge labels outward from the track line. Solve while coding.

**Events**

- Crash: red ring rippling outward on the crash spot, repeating.
- Safety car: bright yellow marker, bigger than cars, dot and ring strobing out of sync like a beacon.

**Pinning and follow**

- Up to 4 pinned cars, each with a flashing halo in a picked colour (shortlist: pink, white, purple, orange - avoids crash red, safety yellow and speed colours). Halos flash out of sync with podium rings; a pinned podium car shows its halo just outside the podium ring.
- Tap a car: a panel shows a colour swatch plus "Follow 30 seconds" and "Stay on". Optionally grey out colours already taken.
- Pin and follow are separate. Follow zooms in and pans to keep one car centred. Default follow car is the user's; if the user has none, the maker's.
- While following, show a clear "Tap to unfollow" button.
- Pins made on the standings board arrive here over ESP-NOW.

**Touch extras:** tap a corner to show its name and record (e.g. "Copse, turn 9"); tap the track to zoom into a section.

**Done when:** a replayed past race shows all cars moving smoothly in the right places on the right map, rings and halos behave as above, layers persist after unplugging, and the maker signs off the look.

## 8. Standings board (left)

The standings board is kept tight: running order, tyres, tyre age, lap count and the top-three feature view. Everything else lives on the drama board.

**Default view:** all drivers in order, each with position, driver, team colour, tyre compound and laps on those tyres (computed, see section 5). Current lap shown, e.g. "Lap 12 / 58".

**Feature view (every so often):** scroll the top 3 one driver at a time, each filling the screen with a big headshot plus car, position and gap. Then back to the full list.

**Touch:**

- Tap a driver: driver card with photo, stats, and race story (started P?, now P?, places gained/lost).
- Tap a driver to pin them: same swatch and follow panel as the map; sends the pin to the map board over ESP-NOW.
- Tap a tyre: tyre age and laps done.
- Tap the top of the list: flip between race order and championship points.

**Images:** 22 driver headshots (319x319 from media.formula1.com, crop to head and shoulders) and 11 team car images. Arvid Lindblad (#41, Racing Bulls) has no official photo: use a plain silhouette with "41" until one appears. Fix the Cadillac black car so it's visible on dark/white backgrounds.

**Done when:** a replayed race shows the correct order, tyres and tyre ages lap by lap; pinning here lights the car up on the map; points flip works; the maker signs off the look.

## 9. Drama board (right)

The drama board shows everything that happens, and is the only board that plays sound by default. It stays calm and classy most of the time; Ryan only speaks for big moments.

**Shows:** current lap, gaps to leader and car ahead, weather (air/track temp, rain), fastest lap and who set it, pit stops, flags and safety car, crashes.

**Incident card:** on a crash, pop up the driver's face and car plus what happened (same style as the standings feature view).

**Battle/crash close-up:** zoom in on a crash or important battle anywhere on track for about 30 seconds, then back to normal. If the battle is still going once the normal info has been readable, jump back to it. The fight for P1 and P2 always comes first.

**Sound (all files done, in Documents > F1 Dashboard > Sounds and Commentary):**

| Trigger | Sound |
| --- | --- |
| Race start | Five-light start sequence + Ryan "lights out and away we go" |
| Crash | Alternate the single-car and multi-car crash clips + Ryan "crash at turn N" (turns 1-20, corner from `race_control`) |
| Safety car | Ryan only (out / coming in / in) - no siren |
| Lead change | Ryan "taken the lead" |
| Overtake (notable) | Ryan "brilliant overtake" |
| Fastest lap | Ryan "fastest lap" |
| Pit stop | Rattle gun (single / double / full stop), Ryan "diving into the pits" |
| Flags | Ryan "yellow flags" / "green flag racing" |
| Finish | Ryan "chequered flag" |
| Battle building | Rising tension clip (to be picked from Freesound) |

Convert all audio to MP3. Freesound credit needed for the rfhache F1 engine clip (CC BY 4.0). No lap-tick sound. Volume slider plus one-tap mute on screen.

**Touch (keepers):** tap a crash to replay its card; tap a pit stop for time, rank and fastest stop of the day; tap fastest lap for who/time/lap and whether someone is closing in; tap weather for detail; "race so far" summary, then tap any moment to open it; tap a named driver to open their card; tap the safety car for reason and laps out; live/quiet toggle; tap for Ryan to comment on the current moment; biggest mover up and down; "what to watch" nudge for a brewing battle. Toy buttons: trigger Ryan, a crash replay, or a fastest-lap celebration.

**Done when:** a replayed race fires the right sound and card at the right moment, nothing talks over itself, mute works, and the maker signs off the look and sound.

## 10. Shared modes: idle, practice, qualifying

The boards switch mode on their own from the session schedule; nobody picks a mode by hand.

**Idle / attract mode (no session running), all 3 screens synced by the map board over ESP-NOW:**

1. Championship top 3, one per screen (leader left, 2nd middle, 3rd right). Each flips driver photo, then car, then stats card. About 2 minutes.
2. AF1 swipe: the AF1 badge (the user's name underneath) sweeps across all three screens as one long white screen. 30-60 s total for steps 2-4.
3. Next race info across the screens: track, date and time, weather.
4. Next three races, one per screen (next, after, after that), each with its own live countdown in NZ time: days, hours, minutes, seconds.
5. Loop.

**Practice:** calm version. Show it's practice, the times and who's quickest. Map shows cars but no drama.

**Qualifying:**

- Middle map: whole track (wide). The car on a flying lap pulses; sectors light up live in timing colours (purple = fastest overall, green = personal best, yellow = slower). Hide the static sector lines during a hot lap so colours don't clash.
- One side screen: zoomed close-up following the hot-lap car; pulls back after the lap. If two cars are on hot laps, pick the higher-placed one (or first to start).
- Other side screen: timing order shuffling, provisional pole, knockout drop zone.

**Race:** full works as in sections 7-9.

**ESP-NOW notes:** use WiFi mode `WIFI_MODE_APSTA` so ESP-NOW and WiFi work together (known fix for packet loss). Map board sends a small "flip to next" message every few seconds in idle mode; also carries pin and follow messages between standings and map. During live sessions all boards follow the shared race clock in the data, so no boss board is needed.

## 11. Build order and checklist

Build one working slice at a time and test on the real boards before moving on. Jobs 1 and 2 are already done.

**Phase 1: Foundation**

- [x] Job 1: text on screen (done)
- [x] Job 2: touch test (done)
- [ ] Job 3: shared `display` + `touch` modules from the LCDwiki code, used by all boards
- [ ] Job 4: WiFi connect/reconnect + NZ time via NTP; clock shows on screen
- [ ] Job 5: `f1data` layer that can replay a past race from free OpenF1 data at real speed (source swappable later)

**Checkpoint A:** The maker sees the correct clock and a replayed race's lap count ticking on a board.

**Phase 2: Map board**

- [ ] Job 6: convert 23 maps to RGB565 headers; load the right map for a session; scroll through maps when idle
- [ ] Job 7: calibrate OpenF1 x/y to each map image (start with Albert Park)
- [ ] Job 8: car icons in team colours moving in replay, plus order strip and tyre icons
- [ ] Job 9: podium rings, battle rings, crash ripple, safety car beacon
- [ ] Job 10: layer toggles + menu icon, saved across power-off
- [ ] Job 11: tap a car: swatch, halo, Follow 30 s / Stay on / Unfollow, zoom-follow

**Checkpoint B:** The maker signs off the map board look during a full replayed race.

**Phase 3: Standings board**

- [ ] Job 12: full list with tyres, tyre age, lap count
- [ ] Job 13: top-3 feature view with photos
- [ ] Job 14: driver card, race story, points flip
- [ ] Job 15: ESP-NOW link; pin on standings shows halo on map

**Checkpoint C:** two boards working together in a replay.

**Phase 4: Drama board**

- [ ] Job 16: check in-box speaker (speaker or buzzer); wire amp if needed; play one MP3
- [ ] Job 17: event feed (gaps, weather, fastest lap, pits, flags)
- [ ] Job 18: event-to-sound mapping and Ryan's lines, no overlapping audio, volume + mute
- [ ] Job 19: incident card and 30 s battle/crash close-up
- [ ] Job 20: drama touch features

**Checkpoint D:** all three boards through a full replayed race; the maker signs off sound.

**Phase 5: Modes and polish**

- [ ] Job 21: idle/attract show synced across three boards
- [ ] Job 22: practice and qualifying modes (hot-lap pulse, live sector colours, close-up screen)
- [ ] Job 23: AF1 badge and secret message for the user
- [ ] Job 24: switch the data source to live (Option A or B, section 5)
- [ ] Job 25: test live at a real practice session, then a race

**Phase 6: Stand (can run alongside, maker)**

- [ ] Print the pit-wall stand v14 on the Bambu, fit boards, speakers, cables and LED strip

**Checkpoint E (finished):** survives a power cut and WiFi drop on its own; runs a full race weekend; the user loves it.

## 12. Assets and where files live

Everything belongs in the maker's Documents > F1 Dashboard folder. A few items are still in Downloads or online and should be moved in.

| Asset | Where it is now | Notes |
| --- | --- | --- |
| Season maps v4 (23 PNG, signed off) | F1 Media folder; also [v4 zip](https://assets.macaly-user-data.dev/t0mna8wnmvqzfjwlwxbyybix/iv53ofeid9nqqivc07ufpy72/iuOtTWOM0yXUS9M5EEhML/f1-season-maps-v4-JuOgHT8t.zip) | Convert to RGB565 headers |
| Driver headshots (22) | F1 Media folder | Crop to head and shoulders; Lindblad placeholder |
| Team car images (11) | F1 Media folder | Fix Cadillac visibility |
| `drivers-2026.csv` | F1 Dashboard folder | All 22 drivers: number, team, colour, photo link |
| `f1-season-wide`, `f1-season-detail` | F1 Dashboard folder | Season calendar and circuit details |
| Sound effects | F1 Dashboard > Sounds | Convert to MP3 |
| Ryan commentary (31 lines) | F1 Dashboard > Commentary | Convert to MP3 |
| LCDwiki demos (29) | Downloads > F1 RACING | Screen driver source; move into F1 Dashboard |
| Spec | Google Drive: "F1 Dashboard - Spec" | 12 sections, 6 Oct 2026 |
| Pit-wall stand v14 | With the maker | 3D print files |

Suggested new folder for the code: Documents > F1 Dashboard > Board Code, with one sketch per board plus the shared library.

## 13. Risks, open questions and small to-dos

The biggest risk is the live data source; everything else can be built and tested on replays first.

| Risk | Impact | What to do |
| --- | --- | --- |
| Live OpenF1 needs a paid account | High | The maker decides Option A or B (section 5) before Job 24 |
| F1 TV live timing is unofficial and may change | Medium | Keep the data layer swappable; Option A as fallback |
| Three boards hit the 30 requests/minute limit | Medium | One board fetches, shares over ESP-NOW |
| ST77922 only works with LCDwiki code | Medium | Wrap it once in the shared `display` module |
| Smooth car movement on a 3.5 in screen | Medium | Draw only changed areas; use PSRAM frame buffer |
| Audio overlapping (two events at once) | Low | Simple queue with priorities: crash > safety car > lead change > others |
| Heat inside the stand | Low | Rear vents already in v14 |

**Open questions for the maker:**

- [ ] Live data: OpenF1 Sponsor account, or F1 TV + helper board?
- [ ] Final four halo colours (leaning pink, white, purple, orange)
- [ ] Speaker on the drama board only, or all three?
- [ ] The secret message for the user: wording

**Small to-dos:**

- [ ] Developer: check if the in-box speaker is a real speaker or a buzzer
- [ ] Pick the battle-building riser sound
- [ ] Confirm Sepang lap count; add the Madrid (Madring) lap record after its first race
- [ ] Design the AF1 badge artwork
- [ ] Buy: USB-C 1-to-4 splitter, right-angle cables, 4-port charger, LED strip, M3 screws
