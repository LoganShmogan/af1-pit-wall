# Hardware notes – LCDwiki ES3C35P

Collected from the Job 1/2 notes, the Map Board and Job 3 READMEs, and the LCDwiki vendor pack
("3.5inch_ESP32-S3-资料包", kept locally and not in this repo because it's the vendor's copyright).

## Board

- ESP32-S3 N16R8: 16 MB flash, 8 MB OPI PSRAM, native USB-C (`/dev/ttyACM0` on Linux Mint).
- 3.5" 320x480 IPS, **ST77922** driver on QSPI, capacitive touch in the same chip.
- Also on the board (from the vendor IO table): ES8311 audio codec, FM8002E speaker amp, MEMS mic,
  WS2812B RGB LED, microSD slot (SDIO), battery ADC.

## Pins

| Function | GPIO | Notes |
| --- | --- | --- |
| LCD QSPI CS / CLK | 10 / 12 | |
| LCD QSPI D0 / D1 / D2 / D3 | 11 / 13 / 14 / 9 | |
| LCD backlight | 41 | high = on (Example_14 does PWM) |
| LCD reset | EN | shared with the board's reset button; no GPIO |
| Touch I2C SDA / SCL | 38 / 39 | 400 kHz, address 0x55. **Shared with the ES8311 codec's control bus** |
| Touch INT / RST | 47 / 48 in our code | **Vendor table says INT = 48, RST = 47.** Touch works as-is (Job 2 passed); swap if touch ever fails to wake |
| Audio amp enable | 1 | low = amp on |
| I2S MCLK / BCLK / LRCK | 17 / 18 / 21 | |
| I2S data to codec / from codec | 16 / 15 | |
| RGB LED (WS2812B) | 40 | |
| Battery ADC | 8 | |
| microSD (SDIO) CLK / CMD / D0–D3 | 5 / 4 / 6, 7, 2, 3 | could hold audio or maps if flash runs out |
| Free | 43, 44 (UART0), 45, 46 | 38/39 free only without touch and audio |

**Sound:** the board already has a codec and amp, so the in-box speaker most likely plugs into the
board's speaker connector and the MAX98357 isn't needed. Confirm with the vendor's
`Example_16_music` (in the vendor pack's .7z) before buying anything (Job 16).

## Screen driver

Arduino_GFX has no ST77922 driver. We use `Arduino_ST77916` over `Arduino_ESP32QSPI` and give it the
ST77922's own init table (converted from the working xiaozhi-esp32 setup for this board) in
Arduino_GFX batch format. Everything is drawn into a full-screen `Arduino_Canvas` in PSRAM, rotation 1
(landscape 480x320), then sent with one `flush()`. Lives in `AF1/src/AF1.cpp`.

Red and blue came out right on all boards. If a future board shows them swapped, send MADCTL `0x36 = 0x08` after `gfx->begin()`. Touch needed no flips either.

## Touch protocol

- Wake: RST low 10 ms, high, wait 100 ms.
- 16-bit registers. `0x0010` status: bit 0x08 = touch present. `0x0014`: first point, 7 bytes.
- Point valid when `b0 & 0x80`. Raw x = `((b0 & 0x3F) << 8) | b1`, raw y = `((b2 & 0x3F) << 8) | b3` (portrait).
- Landscape: `x = raw y`, `y = 319 − raw x`. The flip flags weren't needed.

## Arduino settings (all boards)

| Setting | Value |
| --- | --- |
| Core / libraries | esp32 3.3.12, GFX Library for Arduino 1.6.9, ArduinoJson 7.4.x, PNGdec 1.1.6 (Map Board) |
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | **Enabled** (black screen and silent serial if Disabled) |
| PSRAM | OPI PSRAM |
| Flash Size | 16MB |
| Partition Scheme | Default; **Custom** for the Map Board (`partitions.csv`: 12 MB app, about 3.9 MB SPIFFS) |
| FQBN | `esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi,FlashSize=16M` (+`,PartitionScheme=custom`) |

## History on the real boards

| Step | Result |
| --- | --- |
| Job 1 – name + RGB colour test | Passed on all 3 boards, no colour swap |
| Job 2 – touch dot + 4 corner targets | Passed, no flips |
| Albert Park map test (RGB565 header) | Displayed correctly |
| Map Board v1 – all 23 maps as PNG, tap/swipe | Would have failed: PNGdec 1.1.6 can't decode a 480-wide colour PNG (its line buffer is too small), so maps are now stored as left/right halves. Replaced by `MapBoard/` |
| Job 3 wifi clock, v1 Foundation (replay) | Never run; replaced by `AF1` + `CheckpointA` |
| MapBoard on the middle board (7 Nov 2026) | Runs: the right map, cars on the track, red-flag stop shown correctly; full Mexico replay loads in about 25 s, car positions keep up at 1x |
| StandingsBoard on the left board (7 Nov 2026) | Runs: list, gaps, tyres and driver cards checked on screen; loads in about 16 s |
| DramaBoard on the right board (7 Nov 2026) | Runs after a router restart (see below); all three boards show the same replay moment |

## Network lessons from the real board

- **The ESP32's TLS client doesn't wait for data.** In core 3.3.12, `NetworkClientSecure::readBytes()` returns
  nothing as soon as the next packet hasn't arrived, so streaming JSON straight from it stopped part-way through
  every big reply. `F1Data.cpp` lets `HTTPClient::writeToStream()` collect the whole reply instead.
- **The link to OpenF1 is slow:** about 350 ms round trip from NZ, and the ESP32 only accepts about 5.7 KB per
  round trip, so roughly 15 KB/s. Replies are therefore requested **gzip** (15–30x smaller; OpenF1 only
  compresses over HTTP/1.1) and unzipped with the ESP32's ROM decompressor (`tinfl` in `miniz.h`).

## Flashing and troubleshooting

| What you see | Cause | Fix |
| --- | --- | --- |
| "Sketch too big" / "text section exceeds available space" (map board) | Default partition only has 1.3 MB for the program; the maps need 3.9 MB | Tools > Partition Scheme > **Custom**, Flash Size > **16MB** |
| Black screen and silent serial right after flashing or reading the board with `esptool` | The S3 stayed in download mode; a USB "RTS" reset doesn't bring it out | Press the board's RST button, or `esptool --port /dev/ttyACM0 --after watchdog-reset chip-id` |
| Port is `/dev/ttyACM1` instead of `ttyACM0` | The old port was still held when the board came back | Pick the new port in Tools > Port |
| Serial says "Wifi: still down, trying again" forever, signal fine | The router refuses the board (disconnect reason 2, "auth expired") | Restart the router; check it isn't blocking new devices (MAC filter, device limit) |
| Serial log | 115200 baud | Every OpenF1 request and the "Loaded …" summary are printed |
