// Host unit tests for the AF1 library and the CheckpointA sketch.
// Run: make -C test lib
#include "testing.h"

// The sketch under test (compiled from sketch_lib.cpp)
extern const char *BOARD_NAME;
void draw();
void setup();
void loop();

static void test_parse_iso()
{
  CHECK_EQ(af1ParseIso(nullptr), 0);
  CHECK_EQ(af1ParseIso("2026-11-01"), 0);
  CHECK_EQ(af1ParseIso("2026-11-01T20:03:25+00:00"), 1793563405000);
  CHECK_EQ(af1ParseIso("2026-11-01T20:03:25.510000+00:00"), 1793563405510);
  CHECK_EQ(af1ParseIso("2026-11-01T20:03:25.5+00:00"), 1793563405500);
  CHECK_EQ(af1ParseIso("2024-02-29T23:59:59+00:00"), 1709251199000); // leap day
  CHECK_EQ(af1ParseIso("1999-12-31T00:00:00+00:00"), 946598400000);  // previous 400-year era
}

static void test_display()
{
  fake_gfx_begin_ok = false;
  CHECK(!af1Begin());
  CHECK(fake_serial.find("check PSRAM") != std::string::npos);
  fake_gfx_begin_ok = true;
  CHECK(af1Begin());
  CHECK_EQ(fake_pins[41], HIGH); // backlight
  CHECK_EQ(fake_pins[48], HIGH); // touch out of reset
  af1Backlight(false);
  CHECK_EQ(fake_pins[41], LOW);

  CHECK_EQ(af1Rgb(0xFF, 0x80, 0x00), 0xFC00);
  CHECK_EQ(af1Rgb(0xFF, 0xFF, 0xFF), 0xFFFF);

  gfx->fillScreen(0);
  af1Centred("ABCD", 50, 2, RGB565_WHITE); // 4 chars * 12 px = 48 wide
  CHECK_EQ(gfx->texts.back().x, (480 - 48) / 2);
  CHECK_EQ(gfx->texts.back().y, 50);
  af1Text(5, 6, 3, RGB565_RED, "hi");
  CHECK_EQ(gfx->texts.back().size, 3);
  CHECK_EQ(gfx->texts.back().colour, RGB565_RED);
}

static void test_touch_raw()
{
  resetFakes();
  int x = -1, y = -1;
  CHECK(!af1TouchRaw(x, y)); // no finger
  fake_touch.press(100, 200);
  CHECK(af1TouchRaw(x, y));
  CHECK_EQ(x, 100);
  CHECK_EQ(y, 200);

  fake_touch.nack = true; // chip not answering
  CHECK(!af1TouchRaw(x, y));
  fake_touch.nack = false;
  fake_touch.shortRead = true;
  CHECK(!af1TouchRaw(x, y));
  fake_touch.shortRead = false;
  fake_touch.failReg = 0x0014; // status fine, point read fails
  CHECK(!af1TouchRaw(x, y));
  fake_touch.failReg = 0xFFFF;
  fake_touch.point[0] &= 0x7F; // point not valid
  CHECK(!af1TouchRaw(x, y));

  // Out-of-range numbers are clamped to the screen
  fake_touch.point[0] = 0x80 | 0x3F, fake_touch.point[1] = 0xFF;
  fake_touch.point[2] = 0x3F, fake_touch.point[3] = 0xFF;
  CHECK(af1TouchRaw(x, y));
  CHECK_EQ(x, 479);
  CHECK_EQ(y, 0);
}

// Press at a, optionally drag to b, release; returns the event fired on release.
static TouchEvent gesture(int ax, int ay, int bx, int by)
{
  fake_touch.press(ax, ay);
  CHECK_EQ(af1Touch().kind, T_NONE);
  fake_millis += 50;
  fake_touch.press(bx, by);
  CHECK_EQ(af1Touch().kind, T_NONE);
  fake_touch.release();
  return af1Touch();
}

static void test_touch_gestures()
{
  resetFakes();
  CHECK_EQ(af1Touch().kind, T_NONE); // nothing touched

  TouchEvent e = gesture(100, 120, 105, 125);
  CHECK_EQ(e.kind, T_TAP);
  CHECK_EQ(e.x, 100);
  CHECK_EQ(e.y, 120);
  CHECK_EQ(gesture(300, 100, 200, 110).kind, T_SWIPE_LEFT);
  CHECK_EQ(gesture(200, 100, 300, 110).kind, T_SWIPE_RIGHT);
  CHECK_EQ(gesture(200, 250, 210, 150).kind, T_SWIPE_UP);
  CHECK_EQ(gesture(200, 100, 210, 200).kind, T_SWIPE_DOWN);
  CHECK_EQ(gesture(100, 100, 170, 180).kind, T_SWIPE_DOWN); // diagonal: bigger move wins

  // Hold: fires once after 0.7 s still, then nothing on release
  fake_touch.press(50, 60);
  CHECK_EQ(af1Touch().kind, T_NONE);
  fake_millis += 800;
  e = af1Touch();
  CHECK_EQ(e.kind, T_HOLD);
  CHECK_EQ(e.x, 50);
  CHECK_EQ(af1Touch().kind, T_NONE);
  fake_touch.release();
  CHECK_EQ(af1Touch().kind, T_NONE);

  // Moving finger doesn't count as a hold
  fake_touch.press(50, 60);
  af1Touch();
  fake_millis += 800;
  fake_touch.press(90, 60);
  CHECK_EQ(af1Touch().kind, T_NONE);
  fake_touch.release();
  CHECK_EQ(af1Touch().kind, T_TAP); // 40 px is under the swipe threshold
  fake_touch.press(50, 60);           // same, moving up and down
  af1Touch();
  fake_millis += 800;
  fake_touch.press(50, 100);
  CHECK_EQ(af1Touch().kind, T_NONE);
  fake_touch.release();
  CHECK_EQ(af1Touch().kind, T_TAP);
}

static void test_net()
{
  resetFakes();
  WiFi.st = WL_DISCONNECTED;
  WiFi.begins = WiFi.disconnects = 0;
  fake_tz_calls = 0;
  af1NetBegin("af1-test");
  CHECK_STR(WiFi.hostname, "af1-test");
  CHECK_EQ(WiFi.begins, 1);
  CHECK(!af1WifiOk());

  fake_millis += 5000;
  af1NetTick(); // still within 20 s: just wait
  CHECK_EQ(WiFi.begins, 1);
  fake_millis += 16000;
  af1NetTick(); // 21 s down: start over
  CHECK_EQ(WiFi.disconnects, 1);
  CHECK_EQ(WiFi.begins, 2);

  WiFi.st = WL_CONNECTED;
  af1NetTick();
  CHECK(af1WifiOk());
  CHECK_EQ(fake_tz_calls, 1);
  CHECK(fake_serial.find("Wifi OK, 192.168.1.50") != std::string::npos);
  af1NetTick(); // clock only set up once
  CHECK_EQ(fake_tz_calls, 1);

  // Drops again: the 20 s timer starts from the last time it was up
  WiFi.st = WL_DISCONNECTED;
  fake_millis += 19000;
  af1NetTick();
  CHECK_EQ(WiFi.begins, 2);

  fake_time = 0;
  CHECK(!af1TimeOk());
  fake_time = NOV7;
  CHECK(af1TimeOk());
  CHECK_EQ(af1NowMs(), (int64_t)NOV7 * 1000 + fake_millis);
}

// ---------- Race data (F1Data.cpp) ----------
// A small made-up race on 1 Nov 2026: session 20:00-22:00, lights out 20:03:00.
static const int64_t START = 1793563200000; // 2026-11-01T20:00:00Z
static int64_t at(int h, int m, double s) { return START + ((h - 20) * 3600 + m * 60) * 1000LL + (int64_t)(s * 1000); }
static const int64_t SPAN = (2 * 60 + 30) * 60000LL; // 2 h session + 30 min overrun allowance

static const char *SESSIONS =
    "[{\"session_key\":1,\"date_start\":\"2026-10-26T20:00:00+00:00\",\"date_end\":\"2026-10-26T22:00:00+00:00\","
    "\"session_name\":\"Race\",\"circuit_short_name\":\"Austin\",\"country_name\":\"United States\",\"circuit_key\":9},"
    "{\"session_key\":11404,\"date_start\":\"2026-11-01T20:00:00+00:00\",\"date_end\":\"2026-11-01T22:00:00+00:00\","
    "\"session_name\":\"Race\",\"circuit_short_name\":\"Mexico City\",\"country_name\":\"Mexico\",\"circuit_key\":65},"
    "{\"session_key\":2,\"date_start\":\"2026-10-19T20:00:00+00:00\",\"date_end\":\"2026-10-19T22:00:00+00:00\","
    "\"session_name\":\"Race\",\"circuit_short_name\":\"Older\",\"country_name\":\"X\"}]";
static const char *DRIVERS =
    "[{\"driver_number\":1,\"name_acronym\":\"NOR\",\"team_colour\":\"FF8000\",\"headshot_url\":null},"
    "{\"driver_number\":81,\"name_acronym\":\"PIA\",\"team_colour\":\"FF8000\"},"
    "{\"driver_number\":16,\"name_acronym\":\"LEC\",\"team_colour\":\"E80020\"},"
    "{\"driver_number\":16,\"name_acronym\":\"DUP\",\"team_colour\":\"000000\"},"
    "{\"driver_number\":0,\"name_acronym\":\"BAD\"},{\"driver_number\":100,\"name_acronym\":\"BAD\"},"
    "{\"driver_number\":41}]";
// Lap 3 is missing from the data (so it reads lap 2 until lap 4 starts)
static const char *LAPS =
    "[{\"driver_number\":1,\"lap_number\":1,\"date_start\":\"2026-11-01T20:03:01+00:00\",\"lap_duration\":89.5,\"segments_sector_1\":[2049,2051]},"
    "{\"driver_number\":81,\"lap_number\":1,\"date_start\":\"2026-11-01T20:03:00+00:00\",\"lap_duration\":90},"
    "{\"driver_number\":16,\"lap_number\":1,\"date_start\":\"2026-11-01T20:03:02+00:00\",\"lap_duration\":null},"
    "{\"driver_number\":1,\"lap_number\":2,\"date_start\":\"2026-11-01T20:04:30.500+00:00\",\"lap_duration\":95.0},"
    "{\"driver_number\":81,\"lap_number\":2,\"date_start\":\"2026-11-01T20:04:30+00:00\",\"lap_duration\":88.0},"
    "{\"driver_number\":1,\"lap_number\":4,\"date_start\":\"2026-11-01T20:07:30+00:00\",\"lap_duration\":85.123},"
    "{\"driver_number\":81,\"lap_number\":4,\"date_start\":\"2026-11-01T20:07:31+00:00\",\"lap_duration\":86.0},"
    "{\"driver_number\":1,\"lap_number\":5,\"date_start\":\"2026-11-01T20:08:55+00:00\",\"lap_duration\":84.0},"
    "{\"driver_number\":1,\"lap_number\":2,\"date_start\":null},"
    "{\"driver_number\":1,\"lap_number\":0,\"date_start\":\"2026-11-01T20:00:00+00:00\"},"
    "{\"driver_number\":1,\"lap_number\":256,\"date_start\":\"2026-11-01T20:00:00+00:00\"},"
    "{\"driver_number\":0,\"lap_number\":3,\"date_start\":\"2026-11-01T20:00:00+00:00\"}]";
static const char *POSITIONS =
    "[{\"date\":\"2026-11-01T20:04:00+00:00\",\"driver_number\":1,\"position\":1},"   // NOR passes PIA on lap 1
    "{\"date\":\"2026-11-01T20:04:00+00:00\",\"driver_number\":81,\"position\":2},"
    "{\"date\":\"2026-11-01T19:10:00+00:00\",\"driver_number\":81,\"position\":1},"   // grid (out of order)
    "{\"date\":\"2026-11-01T19:10:00+00:00\",\"driver_number\":1,\"position\":2},"
    "{\"date\":\"2026-11-01T19:10:00+00:00\",\"driver_number\":16,\"position\":3},"
    "{\"date\":\"2026-11-01T20:02:00+00:00\",\"driver_number\":16,\"position\":1},"   // order shuffles before the start
    "{\"date\":\"2026-11-01T20:02:30+00:00\",\"driver_number\":81,\"position\":1},"
    "{\"date\":\"2026-11-01T20:02:30+00:00\",\"driver_number\":16,\"position\":3},"
    "{\"date\":\"2026-11-01T20:04:05+00:00\",\"driver_number\":1,\"position\":1},"    // still leading: not news
    "{\"date\":\"2026-11-01T20:30:00+00:00\",\"driver_number\":99,\"position\":4},"   // driver not in the list
    "{\"date\":\"\",\"driver_number\":1,\"position\":1},"
    "{\"date\":\"2026-11-01T20:05:00+00:00\",\"driver_number\":0,\"position\":1},"
    "{\"date\":\"2026-11-01T20:05:00+00:00\",\"driver_number\":1,\"position\":0},"
    "{\"date\":\"2026-11-01T20:05:00+00:00\",\"driver_number\":1,\"position\":31}]";
static const char *STINTS =
    "[{\"driver_number\":1,\"lap_start\":3,\"compound\":\"HARD\"},"   // later stint listed first
    "{\"driver_number\":1,\"lap_start\":1,\"compound\":\"MEDIUM\",\"tyre_age_at_start\":0},"
    "{\"driver_number\":81,\"lap_start\":1,\"compound\":\"SOFT\"},"
    "{\"driver_number\":0,\"lap_start\":1,\"compound\":\"SOFT\"},{\"driver_number\":16,\"lap_start\":0,\"compound\":\"SOFT\"},"
    "{\"driver_number\":16,\"lap_start\":1,\"compound\":null}]";
static const char *PITS =
    "[{\"date\":\"2026-11-01T19:08:59+00:00\",\"driver_number\":1,\"lap_number\":1,\"pit_duration\":null,\"stop_duration\":null},"
    "{\"date\":\"2026-11-01T20:05:50+00:00\",\"driver_number\":1,\"lap_number\":2,\"pit_duration\":21.9,\"stop_duration\":2.4},"
    "{\"date\":\"2026-11-01T20:06:30+00:00\",\"driver_number\":81,\"lap_number\":3,\"pit_duration\":22.5,\"stop_duration\":null},"
    "{\"date\":\"2026-11-01T20:06:40+00:00\",\"driver_number\":0,\"lap_number\":3,\"pit_duration\":22.5},"
    "{\"date\":\"2026-11-01T20:06:50+00:00\",\"driver_number\":16,\"lap_number\":3,\"stop_duration\":2.1}]";
static std::string rcRow(const char *time, const char *cat, const char *flag, const char *drv, const char *msg)
{
  return std::string("{\"date\":\"2026-11-01T") + time + "+00:00\",\"category\":\"" + cat + "\",\"flag\":" + flag +
         ",\"driver_number\":" + drv + ",\"message\":\"" + msg + "\"}";
}
static std::string raceControl()
{
  return "[" + rcRow("19:20:00", "Flag", "\"GREEN\"", "null", "GREEN LIGHT - PIT EXIT OPEN") + "," +
         rcRow("20:03:26", "Other", "null", "null", "RACE START") + "," +
         rcRow("20:04:10", "Other", "null", "null", "TURN 4 INCIDENT INVOLVING CARS 1 (NOR) AND 16 (LEC) NOTED - CAUSING A COLLISION") + "," +
         rcRow("20:04:15", "Other", "null", "null", "FIA STEWARDS: TURN 4 COLLISION UNDER INVESTIGATION") + "," +
         rcRow("20:04:40", "SafetyCar", "null", "null", "VSC DEPLOYED") + "," +
         rcRow("20:04:50", "SafetyCar", "null", "null", "VIRTUAL SAFETY CAR ENDING") + "," +
         rcRow("20:05:00", "SafetyCar", "null", "null", "SAFETY CAR DEPLOYED") + "," +
         rcRow("20:05:03", "Flag", "\"GREEN\"", "null", "TRACK CLEAR") + "," + // straight from the SC
         rcRow("20:05:05", "SafetyCar", "null", "null", "SAFETY CAR IN THIS LAP") + "," +
         rcRow("20:05:06", "SafetyCar", "null", "null", "VIRTUAL SAFETY CAR DEPLOYED") + "," +
         rcRow("20:05:10", "Flag", "\"GREEN\"", "null", "TRACK CLEAR") + "," +
         rcRow("20:05:20", "Other", "null", "null", "RED FLAG - RACE SUSPENDED") + "," +
         rcRow("20:05:25", "Other", "null", "null", "RACE START") + "," + // resumes from a red flag
         rcRow("20:05:26", "Flag", "\"RED\"", "null", "RED") + "," +
         rcRow("20:05:30", "Other", "null", "null", "FIA STEWARDS: 5 SECOND TIME PENALTY FOR CAR 81 (PIA) - SPEEDING") + "," +
         rcRow("20:05:31", "Other", "null", "16", "10 SECOND TIME PENALTY FOR CAR 16 (LEC)") + "," +
         rcRow("20:05:32", "Other", "null", "null", "FIA STEWARDS: PENALTY SERVED - 5 SECOND TIME PENALTY FOR CAR 81 (PIA)") + "," +
         rcRow("20:05:40", "Other", "null", "16", "CAR 16 (LEC) STOPPED") + "," +
         rcRow("20:05:41", "Other", "null", "null", "CAR 81 (PIA) CRASH AT TURN 300") + "," +
         rcRow("20:05:42", "Other", "null", "null", "CONTACT TURN 2 CARS 99 AND 0") + "," +
         rcRow("20:05:43", "Other", "null", "null", "CONTACT SOMEWHERE") + "," +
         rcRow("20:05:44", "Flag", "\"YELLOW\"", "null", "YELLOW IN TRACK SECTOR 3") + "," +
         rcRow("20:05:45", "Other", "null", "null", "CAR 1 (NOR) TIME 1:22.907 DELETED - TRACK LIMITS AT TURN 12") + "," +
         rcRow("20:09:00", "Flag", "\"CHEQUERED\"", "null", "CHEQUERED FLAG") + "]";
}
static const char *WEATHER =
    "[{\"date\":\"2026-11-01T19:10:00+00:00\",\"air_temperature\":22.5,\"track_temperature\":44.3,\"humidity\":44.4,\"rainfall\":0},"
    "{\"date\":\"2026-11-01T20:06:00+00:00\",\"air_temperature\":21,\"track_temperature\":40.5,\"humidity\":60,\"rainfall\":1}]";

static std::string many(int n, std::string (*row)(int))
{
  std::string s = "[";
  for (int i = 0; i < n; i++) s += (i ? "," : "") + row(i);
  return s + "]";
}

static const uint16_t ALL_LOADS = F1_POSITIONS | F1_LAPS | F1_STINTS | F1_PITS | F1_RACE_CONTROL | F1_WEATHER;

static void routeRace()
{
  route("sessions?", 200, SESSIONS);
  route("drivers?", 200, DRIVERS);
  route("laps?", 200, LAPS);
  route("position?", 200, POSITIONS);
  route("stints?", 200, STINTS);
  route("pit?", 200, PITS);
  route("race_control?", 200, raceControl());
  route("weather?", 200, WEATHER);
  route("intervals?", 404, "");
  route("location?", 404, "");
}

// Tick until loaded (one request per tick, spaced 600 ms apart). Stops as soon as it's ready.
static void loadAll()
{
  for (int i = 0; i < 20 && !race.ready; i++)
  {
    f1Tick();
    if (!race.ready) fake_millis += 600;
  }
}

// Move the clock on like a real board: a tick every second
static void advance(uint32_t ms)
{
  for (; ms >= 1000; ms -= 1000)
  {
    fake_millis += 1000;
    f1Tick();
  }
  fake_millis += ms;
  f1Tick();
}

// Load the race and put the replay clock at `t`. `extra` can add routes first (first match wins).
static void loadAt(uint16_t feeds, int64_t t, void (*extra)() = nullptr)
{
  resetFakes();
  if (extra) extra();
  routeRace();
  f1Begin(0, 1, feeds);
  loadAll();
  f1Restart();
  fake_millis += (uint32_t)(t - START);
  f1Tick();
}

static std::vector<const F1News *> newsSince(uint32_t from)
{
  std::vector<const F1News *> r;
  for (uint32_t i = from; i < race.newsCount; i++) r.push_back(f1News(i));
  return r;
}

static bool hasNews(uint32_t from, F1NewsKind k, const char *text = "")
{
  for (auto n : newsSince(from))
    if (n && n->kind == k && strstr(n->text, text)) return true;
  return false;
}

static void test_loading()
{
  resetFakes();
  routeRace();
  f1Begin(0, 1, ALL_LOADS);
  CHECK(!race.ready);
  f1SetSpeed(10); // before loading: only remembers the speed
  f1Restart();    // before loading: nothing to do
  f1SetSpeed(1);

  WiFi.st = WL_DISCONNECTED; // waits for wifi
  f1Tick();
  CHECK(fake_requests.empty());
  WiFi.st = WL_CONNECTED;
  fake_time = 0; // and for the clock
  f1Tick();
  CHECK(fake_requests.empty());
  fake_time = NOV7;

  f1Tick();
  CHECK_EQ(fake_requests.size(), 1u);
  CHECK(fake_requests[0].find("sessions?year=2026&session_name=Race&date_end<2026-11-07T08:05:09") != std::string::npos);
  f1Tick(); // too soon for the next request
  CHECK_EQ(fake_requests.size(), 1u);
  loadAll();
  CHECK(race.ready);
  CHECK_EQ(fake_requests.size(), 8u); // sessions, drivers + 6 feeds
  CHECK(fake_requests[7].find("weather?session_key=11404") != std::string::npos);
  CHECK_STR(race.status, "");
  CHECK(fake_serial.find("Loaded Mexico City Race: 4 drivers, 5 laps") != std::string::npos);

  CHECK_EQ(race.sessionKey, 11404);
  CHECK_EQ(race.circuitKey, 65);
  CHECK_STR(race.circuit, "Mexico City");
  CHECK_STR(race.country, "Mexico");
  CHECK_STR(race.sessionName, "Race");
  CHECK_EQ(race.startMs, START);
  CHECK_EQ(race.endMs, START + SPAN);
  CHECK_EQ(race.totalLaps, 5);

  CHECK_EQ(race.driverCount, 4);
  CHECK_STR(f1Driver(1)->code, "NOR");
  CHECK_EQ(f1Driver(1)->colour, af1Rgb(0xFF, 0x80, 0x00));
  CHECK_STR(f1Driver(16)->code, "LEC");
  CHECK_STR(f1Driver(41)->code, "???");
  CHECK_EQ(f1Driver(41)->colour, 0xFFFF);
  CHECK(f1Driver(99) == nullptr);

  // Feeds this board didn't ask for aren't fetched
  resetFakes();
  routeRace();
  f1Begin(11404, 1, F1_LAPS);
  loadAll();
  CHECK(race.ready);
  CHECK_EQ(fake_requests.size(), 3u);
  CHECK(fake_requests[0].find("sessions?session_key=11404") != std::string::npos);
}

static void test_replay_sync()
{
  // Two boards started at different times land on the same race moment
  resetFakes();
  routeRace();
  f1Begin(0, 1, F1_LAPS);
  loadAll();
  f1Tick();
  int64_t first = race.clockMs;
  int64_t expect = START + ((int64_t)NOV7 * 1000 + fake_millis) % SPAN;
  CHECK_EQ(first, expect);

  // Restart: back to the session start, then the clock runs at the replay speed
  f1Restart();
  f1Tick();
  CHECK_EQ(race.clockMs, START);
  fake_millis += 2000;
  f1Tick();
  CHECK_EQ(race.clockMs, START + 2000);
  f1SetSpeed(60); // keeps the moment
  f1Tick();
  CHECK_EQ(race.clockMs, START + 2000);
  fake_millis += 1000;
  f1Tick();
  CHECK_EQ(race.clockMs, START + 62000);
  f1SetSpeed(1);

  // Loops round after the session (+30 min) and replays from the top
  fake_millis += (uint32_t)(SPAN - 62000);
  f1Tick();
  CHECK_EQ(race.clockMs, START);
  CHECK_EQ(race.lap, 0);

  // A session with no end date still plays (30 minutes)
  resetFakes();
  route("sessions?", 200, "[{\"session_key\":5,\"date_start\":\"2026-11-01T20:00:00+00:00\",\"date_end\":null}]");
  routeRace();
  f1Begin(0, 1, F1_LAPS);
  loadAll();
  CHECK(race.ready);
  CHECK_EQ(race.endMs, START + 30 * 60000);
}

static void test_race_story()
{
  // On the grid
  loadAt(ALL_LOADS, START);
  uint32_t seen = race.newsCount;
  CHECK_EQ(race.lap, 0);
  CHECK_EQ(race.track, TRACK_GREEN); // pit exit opened at 19:20
  CHECK_STR(f1DriverAt(1)->code, "PIA");
  CHECK_STR(f1DriverAt(2)->code, "NOR");
  CHECK(f1DriverAt(4) == nullptr);
  CHECK_EQ(f1Driver(41)->pos, 0);
  CHECK(race.hasWeather);
  CHECK(race.airC > 22.4f && race.airC < 22.6f);
  CHECK_EQ(race.humidity, 44);
  CHECK(!race.raining);
  CHECK_EQ(f1Driver(1)->compound, 'M'); // first set, before lap 1
  CHECK_EQ(f1Driver(1)->tyreLaps, 0);

  // 20:03:00 lights out (earliest lap-1 time): grid recorded, START news
  advance(180000);
  CHECK_EQ(race.lap, 1);
  CHECK(hasNews(seen, NEWS_START, "Lights out"));
  CHECK_EQ(f1Driver(81)->gridPos, 1);
  CHECK_EQ(f1Driver(1)->gridPos, 2);
  CHECK_EQ(f1Driver(81)->compound, 'S');
  CHECK_EQ(f1Driver(81)->tyreLaps, 1);
  CHECK_EQ(f1Driver(16)->compound, 0); // no valid stint for LEC
  CHECK_EQ(f1Driver(16)->tyreLaps, 0);

  // 20:04:00 NOR takes the lead; 20:04:10 collision at turn 4
  seen = race.newsCount;
  advance(70000);
  CHECK_STR(f1DriverAt(1)->code, "NOR");
  CHECK(hasNews(seen, NEWS_LEAD, "NOR takes the lead from PIA"));
  const F1News *crash = nullptr;
  for (auto n : newsSince(seen))
    if (n->kind == NEWS_CRASH) crash = n;
  CHECK(crash != nullptr);
  CHECK_EQ(crash->drv, 1);
  CHECK_EQ(crash->drv2, 16);
  CHECK_EQ(crash->turn, 4);
  CHECK_EQ(crash->lap, 1);
  CHECK_EQ(newsSince(seen).size(), 2u); // the stewards' follow-up isn't news

  // 20:04:30.5 lap 2 (lap times: PIA 90.0 at 20:04:30, NOR 89.5 at 20:04:30.5)
  advance(20500);
  CHECK_EQ(race.lap, 2);
  CHECK_EQ(f1Driver(1)->lastLapMs, 89500);
  CHECK_EQ(race.fastestLapMs, 89500);
  CHECK_EQ(race.fastestLapDrv, 1);
  CHECK_EQ(race.fastestLapNo, 1);
  CHECK(!hasNews(seen, NEWS_FASTEST)); // too early in the race to shout about

  // 20:04:40 VSC, 20:04:50 ending
  seen = race.newsCount;
  advance(9500);
  CHECK_EQ(race.track, TRACK_VSC);
  CHECK(hasNews(seen, NEWS_VSC));
  advance(10000);
  CHECK_EQ(race.track, TRACK_GREEN);
  CHECK(hasNews(seen, NEWS_SC_ENDING));

  // 20:05:00 SC, 20:05:05 in this lap, 20:05:06 VSC again, 20:05:10 green flag -> racing resumes
  seen = race.newsCount;
  advance(10000);
  CHECK_EQ(race.track, TRACK_SC);
  CHECK_STR(f1TrackName(race.track), "SAFETY CAR");
  advance(10000);
  CHECK(hasNews(seen, NEWS_SC));
  CHECK(hasNews(seen, NEWS_SC_ENDING));
  CHECK(hasNews(seen, NEWS_VSC));
  CHECK(hasNews(seen, NEWS_GREEN, "TRACK CLEAR"));
  CHECK_EQ(race.track, TRACK_GREEN);

  // 20:05:20 red flag, 20:05:25 restart, 20:05:26 red again
  seen = race.newsCount;
  advance(10000);
  CHECK_EQ(race.track, TRACK_RED);
  CHECK_STR(f1TrackName(race.track), "RED FLAG");
  advance(5000);
  CHECK(hasNews(seen, NEWS_GREEN, "RACE START"));
  advance(1000);
  CHECK_EQ(race.track, TRACK_RED);

  // 20:05:30-45: penalties, stopped car, crashes, noise that isn't news
  seen = race.newsCount;
  advance(20000);
  auto items = newsSince(seen);
  CHECK_EQ(items.size(), 6u);
  CHECK(items.size() == 6 && items[0]->kind == NEWS_PENALTY && items[0]->drv == 81);
  CHECK(items.size() == 6 && items[1]->kind == NEWS_PENALTY && items[1]->drv == 16);
  CHECK(items.size() == 6 && items[2]->kind == NEWS_CRASH && items[2]->drv == 16 && items[2]->turn == 0);
  CHECK(items.size() == 6 && items[3]->kind == NEWS_CRASH && items[3]->drv == 81 && items[3]->turn == 0); // turn 300 isn't real
  CHECK(items.size() == 6 && items[4]->kind == NEWS_CRASH && items[4]->drv == 99 && items[4]->drv2 == 0 && items[4]->turn == 2);
  CHECK(items.size() == 6 && items[5]->kind == NEWS_CRASH && items[5]->drv == 0 && items[5]->turn == 0);

  // 20:05:50 NOR pits (2.4 s stop), 20:06:00 rain, 20:06:30 PIA pits (22.5 s lane time)
  seen = race.newsCount;
  advance(50000);
  CHECK(hasNews(seen, NEWS_PIT, "NOR pits - 2.4 s"));
  CHECK(hasNews(seen, NEWS_PIT, "PIA pits - 22.5 s"));
  CHECK_EQ(f1Driver(1)->pitStops, 1);
  CHECK_EQ(race.fastestPitMs, 2400);
  CHECK_EQ(race.fastestPitDrv, 1);
  CHECK(race.raining);
  CHECK_EQ(race.lap, 2); // lap 3 isn't in the data

  // 20:06:05 NOR's slow lap 2 (95.0) and PIA's 88.0 at 20:05:58
  CHECK_EQ(f1Driver(81)->bestLapMs, 88000);
  CHECK_EQ(f1Driver(1)->bestLapMs, 89500); // 95.0 wasn't better
  CHECK_EQ(race.fastestLapDrv, 81);

  // 20:07:30 lap 4: red flag over, NOR on the hard tyre (stint from lap 3)
  advance(60000);
  CHECK_EQ(race.lap, 4);
  CHECK_EQ(race.track, TRACK_GREEN);
  CHECK_EQ(f1Driver(1)->compound, 'H');
  CHECK_EQ(f1Driver(1)->tyreLaps, 2);
  CHECK_EQ(race.fastestPitMs, 2100); // LEC's 2.1 s at 20:06:50
  CHECK_EQ(race.fastestPitDrv, 16);

  // 20:08:55 NOR fastest lap 85.123 on lap 4 -> news (lap >= 3); 20:09:00 chequered
  seen = race.newsCount;
  advance(92000);
  CHECK(hasNews(seen, NEWS_FASTEST, "Fastest lap: NOR 1:25.123"));
  CHECK_EQ(race.fastestLapNo, 4);
  CHECK_EQ(race.track, TRACK_CHEQUERED);
  CHECK(hasNews(seen, NEWS_FINISH));
  CHECK_STR(f1TrackName(race.track), "CHEQUERED FLAG");
  CHECK_STR(f1TrackName((F1Track)99), "");

  // Restart: everything back to the grid, the news count keeps going up
  uint32_t before = race.newsCount;
  f1Restart();
  f1Tick();
  CHECK_EQ(race.lap, 0);
  CHECK_EQ(race.track, TRACK_GREEN);
  CHECK_EQ(race.fastestLapMs, 0);
  CHECK_EQ(f1Driver(1)->pitStops, 0);
  CHECK_STR(f1DriverAt(1)->code, "PIA");
  CHECK_EQ(race.newsCount, before);
}

static void test_join_mid_race()
{
  // A board that starts at 20:10 gets the state but no flood of old news
  uint32_t before = race.newsCount;
  loadAt(ALL_LOADS, at(20, 10, 0));
  CHECK_EQ(race.lap, 5);
  CHECK_EQ(race.track, TRACK_CHEQUERED);
  CHECK_EQ(race.fastestLapMs, 85123);
  CHECK_EQ(f1Driver(1)->pitStops, 1);
  CHECK_EQ(f1Driver(81)->gridPos, 1);
  CHECK_EQ(race.newsCount, before);
}

static void test_news_ring()
{
  resetFakes();
  f1Begin(0, 1, 0);
  uint32_t base = race.newsCount;
  CHECK(f1News(base) == nullptr); // not happened yet
  for (int i = 0; i < F1_NEWS_KEEP + 3; i++)
  {
    F1News n = {};
    n.kind = NEWS_PENALTY;
    snprintf(n.text, sizeof(n.text), "item %d", i);
    f1PushNews(n);
  }
  CHECK(f1News(base) == nullptr); // too old
  CHECK(f1News(base + 3) != nullptr);
  CHECK_EQ(f1News(base + 3)->seq, base + 3);
  CHECK_STR(f1News(base + F1_NEWS_KEEP + 2)->text, "item 34");
}

// Intervals and locations: streamed in windows just ahead of the clock
static std::string ivRow(const char *time, int drv, const char *gap, const char *iv)
{
  return std::string("{\"date\":\"2026-11-01T") + time + "+00:00\",\"driver_number\":" + std::to_string(drv) +
         ",\"gap_to_leader\":" + gap + ",\"interval\":" + iv + "}";
}
static std::string locRow(const char *time, int drv, int x, int y)
{
  return std::string("{\"date\":\"2026-11-01T") + time + "+00:00\",\"driver_number\":" + std::to_string(drv) +
         ",\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y) + ",\"z\":100}";
}

static void setRoute(const char *match, int code, const std::string &body)
{
  for (auto &r : fake_routes)
    if (r.first == match) r.second = {code, body};
}

static void intervalRoutes()
{
  route("intervals?", 200,
        "[" + ivRow("20:03:31", 1, "0", "0") + "," + ivRow("20:03:31", 81, "1.234", "1.234") + "," +
            ivRow("20:03:31", 16, "\"+1 LAP\"", "\"+1 LAP\"") + "," + ivRow("20:03:31", 41, "null", "700.5") + "," +
            ivRow("20:03:31", 0, "1", "1") + "," + ivRow("", 1, "1", "1") + "]");
}

static void test_intervals()
{
  // The first window is fetched as soon as the replay runs
  loadAt(F1_POSITIONS | F1_INTERVALS, at(20, 3, 30), intervalRoutes);
  CHECK(fake_requests.back().find("intervals?session_key=11404&date>=2026-11-01T20:03:28.000&date<2026-11-01T20:04:28.000") != std::string::npos);
  CHECK_EQ(f1Driver(1)->gapMs, -1); // 20:03:31 not reached yet
  CHECK_EQ(f1Driver(1)->intervalMs, -1);
  fake_millis += 1000;
  f1Tick();
  CHECK_EQ(f1Driver(1)->gapMs, 0);
  CHECK_EQ(f1Driver(1)->intervalMs, 0);
  CHECK_EQ(f1Driver(81)->gapMs, 1234);
  CHECK_EQ(f1Driver(81)->intervalMs, 1230); // 10 ms steps
  CHECK_EQ(f1Driver(16)->lapsDown, 1);
  CHECK_EQ(f1Driver(16)->gapMs, -1);
  CHECK_EQ(f1Driver(16)->intervalMs, -1);
  CHECK_EQ(f1Driver(41)->gapMs, -1);
  CHECK_EQ(f1Driver(41)->lapsDown, 0);
  CHECK_EQ(f1Driver(41)->intervalMs, -1); // too big to store

  // Keeps a window ahead: the next one now (an empty 404 window is fine), then nothing for a while
  setRoute("intervals?", 404, "");
  size_t n = fake_requests.size();
  fake_millis += 1000; // 2 s since the last request
  f1Tick();
  CHECK_EQ(fake_requests.size(), n + 1);
  CHECK(fake_requests.back().find("date>=2026-11-01T20:04:28.000") != std::string::npos);
  CHECK_STR(race.status, "");
  fake_millis += 3000;
  f1Tick();
  CHECK_EQ(fake_requests.size(), n + 1);

  // Errors wait and retry the same window
  setRoute("intervals?", 429, "");
  fake_millis += 60000; // clock 20:04:35, the window from 20:05:28 is due
  f1Tick();
  CHECK(strstr(race.status, "Too many requests") != nullptr);
  n = fake_requests.size();
  fake_millis += 29000;
  f1Tick();
  CHECK_EQ(fake_requests.size(), n);
  fake_millis += 1000;
  f1Tick();
  CHECK_EQ(fake_requests.size(), n + 1);
  CHECK(fake_requests.back().find("date>=2026-11-01T20:05:28.000") != std::string::npos);

  // Clock jumps back (restart): windows start again from the new moment
  setRoute("intervals?", 404, "");
  f1Restart();
  fake_millis += 30000;
  f1Tick();
  CHECK(fake_requests.back().find("date>=2026-11-01T20:00:28.000") != std::string::npos);

  // Nothing after the end of the replay
  f1Restart();
  fake_millis += (uint32_t)(SPAN - 5000);
  f1Tick();
  n = fake_requests.size();
  fake_millis += 2100;
  f1Tick();
  CHECK_EQ(fake_requests.size(), n);
}

static void locationRoutes()
{
  route("location?", 200,
        "[" + locRow("20:03:31.000", 1, 100, 200) + "," + locRow("20:03:31.500", 1, 200, -300) + "," +
            locRow("20:03:32.000", 1, 300, -400) + "," + locRow("20:03:32.500", 1, 400, -500) + "," +
            locRow("20:03:33.000", 1, 500, -600) + "," + locRow("20:03:31.000", 81, 0, 0) + "," +
            locRow("20:03:31.000", 81, 50, 0) + "," + locRow("20:03:31.000", 99, 5, 5) + "," +
            locRow("20:03:31.000", 16, 0, 7) + "]");
}
static void lateLocationRoutes()
{
  route("location?", 200, "[" + locRow("20:03:31.900", 1, 10, 20) + "," + locRow("20:03:32.000", 1, 30, 40) + "]");
}
static void restartDuringDownload()
{
  if (fake_requests.back().find("location?") == std::string::npos) return;
  fake_on_get = nullptr;
  WiFi.st = WL_DISCONNECTED; // so the nested f1Tick only plays
  f1Restart();
  f1Tick();
  WiFi.st = WL_CONNECTED;
}
static void restartRoutes()
{
  route("location?", 200, "[" + locRow("20:00:01.000", 1, 100, 200) + "]");
  fake_on_get = restartDuringDownload;
}

static void test_locations()
{
  loadAt(F1_POSITIONS | F1_LOCATION, at(20, 3, 30), locationRoutes);
  CHECK(fake_requests.back().find("location?session_key=11404&date>=2026-11-01T20:03:28.000&date<2026-11-01T20:03:43.000") != std::string::npos);
  CHECK(!f1Driver(1)->hasLocation);
  fake_millis += 2500; // clock 20:03:32.5, cars drawn at 20:03:31.9
  f1Tick();
  CHECK(f1Driver(1)->hasLocation);
  CHECK_EQ(f1Driver(1)->x, 280); // between 200 (31.5) and 300 (32.0)
  CHECK_EQ(f1Driver(1)->y, -380);
  CHECK(f1Driver(81)->hasLocation); // the (0,0) "not on track" sample was skipped
  CHECK_EQ(f1Driver(81)->x, 50);
  CHECK(f1Driver(16)->hasLocation); // x = 0 is a real place on track
  CHECK_EQ(f1Driver(16)->y, 7);
  fake_millis += 2000; // past the last sample: stays there (5 samples, the oldest was dropped)
  f1Tick();
  CHECK_EQ(f1Driver(1)->x, 500);
  CHECK_EQ(f1Driver(1)->y, -600);

  // Drawn before a car's first sample: shows the first sample
  loadAt(F1_POSITIONS | F1_LOCATION, at(20, 3, 30), lateLocationRoutes);
  fake_millis += 2000; // clock 20:03:32, drawn at 20:03:31.4
  f1Tick();
  CHECK(f1Driver(1)->hasLocation);
  CHECK_EQ(f1Driver(1)->x, 10);

  // Clock restarted while a window was downloading: that window is thrown away
  loadAt(F1_POSITIONS | F1_LOCATION, at(20, 3, 30), restartRoutes);
  fake_millis += 2000; // clock 20:00:02: the sample at 20:00:01 would show if it had been kept
  f1Tick();
  CHECK(!f1Driver(1)->hasLocation);
}

static void bufferRoutes()
{
  route("location?", 200, many(5000, [](int i) { return locRow("20:03:40.000", 1, i + 1, i + 1); }));
}

static void test_stream_buffer_full()
{
  // Window 1: 5000 rows still in the future; window 2: more rows than the buffer has room for
  loadAt(F1_POSITIONS | F1_LOCATION, at(20, 3, 30), bufferRoutes);
  setRoute("location?", 200, many(16500, [](int i) {
             char t[24];
             snprintf(t, sizeof(t), "20:03:%02d.%03d", 43 + i / 1000, i % 1000);
             return locRow(t, 1, i + 1, i + 1);
           }));
  size_t n = fake_requests.size();
  fake_millis += 2000;
  f1Tick();
  CHECK_EQ(fake_requests.size(), n + 1);
  // 16384 rows staged, 11384 fit: the next window starts where it stopped (20:03:54.384)
  setRoute("location?", 404, "");
  fake_millis += 9000; // clock 20:03:41: still more than half full, no request
  f1Tick();
  CHECK_EQ(fake_requests.size(), n + 1);
  fake_millis += 6000; // clock 20:03:47: drained below half
  f1Tick();
  CHECK_EQ(fake_requests.size(), n + 2);
  CHECK(fake_requests.back().find("date>=2026-11-01T20:03:54.384") != std::string::npos);
}

static void expectFailure(const char *endpoint, int code, const char *body, const char *status, uint32_t retryMs)
{
  resetFakes();
  route(endpoint, code, body);
  routeRace();
  f1Begin(0, 1, ALL_LOADS);
  for (int i = 0; i < 12 && !race.status[0]; i++) // stop right after the failing request
  {
    fake_millis += 600;
    f1Tick();
  }
  CHECK(!race.ready);
  if (!strstr(race.status, status)) printf("  status was: %s\n", race.status);
  CHECK(strstr(race.status, status) != nullptr);
  // Retries after the wait, not before
  size_t n = fake_requests.size();
  fake_millis += retryMs - 1;
  f1Tick();
  CHECK_EQ(fake_requests.size(), n);
  fake_millis += 1;
  f1Tick();
  CHECK_EQ(fake_requests.size(), n + 1);
}

static void test_load_errors()
{
  expectFailure("sessions?", 401, "", "refused sessions (401)", 60000);
  expectFailure("sessions?", 403, "", "refused sessions (403)", 60000);
  expectFailure("drivers?", 429, "", "Too many requests", 30000);
  expectFailure("laps?", 500, "", "OpenF1 error 500 loading laps", 10000);
  expectFailure("race_control?", 500, "", "loading race control", 10000);
  expectFailure("sessions?", 200, "[]", "No session data", 10000);
  expectFailure("sessions?", 404, "", "No sessions data", 10000);
  expectFailure("sessions?", 200, "{\"detail\":\"oops\"}", "No session data", 10000);
  expectFailure("sessions?", 200, "[{\"session_key\":}]", "No session data", 10000); // broken JSON

  // A feed with nothing in it (404) is fine
  resetFakes();
  route("pit?", 404, "");
  routeRace();
  f1Begin(0, 1, ALL_LOADS);
  loadAll();
  CHECK(race.ready);

  // A half-loaded feed is dropped and fetched again
  resetFakes();
  route("weather?", 500, "");
  routeRace();
  f1Begin(0, 1, F1_POSITIONS | F1_WEATHER);
  loadAll();
  CHECK(!race.ready);
  setRoute("weather?", 200, WEATHER);
  fake_millis += 10000;
  loadAll();
  CHECK(race.ready);
  CHECK(fake_serial.find("4 drivers, 0 laps, 12 events") != std::string::npos); // 10 positions + 2 weather

  // Nothing answers at all
  resetFakes();
  f1Begin(0, 1, ALL_LOADS);
  f1Tick();
  CHECK(strstr(race.status, "Can't reach OpenF1 (connection refused)") != nullptr);
  resetFakes();
  routeRace();
  fake_http_begin_ok = false;
  f1Begin(0, 1, ALL_LOADS);
  f1Tick();
  CHECK(strstr(race.status, "Can't reach OpenF1") != nullptr);
}

static std::string loadedLine()
{
  size_t i = fake_serial.find("Loaded ");
  return i == std::string::npos ? "" : fake_serial.substr(i, fake_serial.find('\n', i) - i);
}

// OpenF1 sends gzip when asked (15-30x smaller, which the board's slow link needs)
static void test_gzip()
{
  resetFakes();
  routeRace();
  f1Begin(0, 1, ALL_LOADS);
  loadAll();
  std::string plain = loadedLine();
  CHECK_STR(fake_accept_encoding, "gzip");

  resetFakes();
  fake_gzip = true;
  routeRace();
  f1Begin(0, 1, ALL_LOADS);
  loadAll();
  CHECK(race.ready);
  CHECK(!plain.empty());
  CHECK_STR(loadedLine(), plain); // exactly the same race

  // Spaces between items are fine; a reply that stops part-way keeps what arrived
  resetFakes();
  route("drivers?", 200, "[ {\"driver_number\":1} ,\n {\"driver_number\":2} ]");
  route("laps?", 200, "[{\"driver_number\":1,\"lap_number\":7,\"date_start\":\"2026-11-01T20:03:01+00:00\"}");
  routeRace();
  f1Begin(0, 1, F1_LAPS);
  loadAll();
  CHECK(race.ready);
  CHECK_EQ(race.driverCount, 2);
  CHECK_EQ(race.totalLaps, 7);
}

// Damaged gzip replies are errors (and are fetched again), never a crash
static void expectBadReply(const std::string &body, int writeError, const char *status = "Can't reach OpenF1")
{
  resetFakes();
  route("sessions?", 200, body);
  routeRace();
  fake_write_error = writeError;
  f1Begin(0, 1, F1_LAPS);
  f1Tick();
  CHECK(!race.ready);
  CHECK(strstr(race.status, status) != nullptr);
}

static void test_bad_replies()
{
  std::string good = fakeGzip(SESSIONS);
  std::string cut = good.substr(0, good.size() - 30); // connection dropped part-way
  std::string flags = good;
  flags[3] = 8; // has a file name field
  std::string huge = good;
  huge[huge.size() - 1] = 0x7F; // claims to be ~2 GB
  expectBadReply(cut, 0);
  expectBadReply(flags, 0);
  expectBadReply(good.substr(0, 12), 0); // too short to be gzip
  expectBadReply(huge, 0);
  expectBadReply(SESSIONS, HTTPC_ERROR_READ_TIMEOUT);
  expectBadReply("\x1f", 0, "No session data");  // one byte: not gzip, not JSON either: an empty reply
  expectBadReply("\x1fX", 0, "No session data"); // starts like gzip but isnt
}

static void test_limits()
{
  // More rows than there's room for: the extra ones are dropped, nothing breaks
  resetFakes();
  route("drivers?", 200, many(30, [](int i) { return "{\"driver_number\":" + std::to_string(i + 1) + "}"; }));
  route("position?", 200, many(12010, [](int i) {
          return "{\"date\":\"2026-11-01T20:10:00+00:00\",\"driver_number\":1,\"position\":" + std::to_string(i % 20 + 1) + "}";
        }));
  route("stints?", 200, many(210, [](int) { return std::string("{\"driver_number\":1,\"lap_start\":1,\"compound\":\"SOFT\"}"); }));
  route("race_control?", 200, many(410, [](int) { return rcRow("20:10:00", "Other", "null", "null", "HELLO"); }));
  route("weather?", 200, many(410, [](int) {
          return std::string("{\"date\":\"2026-11-01T20:10:00+00:00\",\"air_temperature\":20}");
        }));
  routeRace();
  f1Begin(0, 1, F1_POSITIONS | F1_STINTS | F1_RACE_CONTROL | F1_WEATHER);
  loadAll();
  CHECK(race.ready);
  CHECK_EQ(race.driverCount, 24);
  CHECK(fake_serial.find("12000 events") != std::string::npos);
}

static void manyLocationRoutes()
{
  route("location?", 200, many(16400, [](int i) { return locRow("20:03:35.000", 1, i + 1, 0); }));
}

static void test_more_locations()
{
  // Staging holds at most 16384 rows of one window
  loadAt(F1_POSITIONS | F1_LOCATION, at(20, 3, 30), manyLocationRoutes);
  fake_millis += 6000;
  f1Tick();
  CHECK(f1Driver(1)->hasLocation);
  CHECK_EQ(f1Driver(1)->x, 16384); // last staged row
}

static void test_checkpoint_sketch()
{
  resetFakes();
  routeRace();
  WiFi.st = WL_DISCONNECTED;
  fake_time = 0;
  setup();
  CHECK_STR(WiFi.hostname, "af1-MAP");

  draw();
  CHECK(gfx->shows("MAP"));
  CHECK(gfx->shows("NO WIFI"));
  CHECK(gfx->shows("Joining wifi..."));
  CHECK(gfx->shows("Loading race data..."));

  WiFi.st = WL_CONNECTED;
  draw();
  CHECK(gfx->shows("Getting the time..."));
  fake_time = NOV7;
  fake_local_ok = false; // time set but local time not ready yet
  draw();
  CHECK(gfx->shows("Getting the time..."));
  fake_local_ok = true;

  draw();
  CHECK(gfx->shows("8:05:09 am"));
  CHECK(gfx->shows("Saturday 7 November"));
  fake_time = NOV7 - 8 * 3600; // 00:05 -> 12 am
  draw();
  CHECK(gfx->shows("12:05:09 am"));
  fake_time = NOV7 + 5 * 3600; // 13:05 -> 1 pm
  draw();
  CHECK(gfx->shows("1:05:09 pm"));
  fake_time = NOV7;

  // Error message shows along the bottom
  strcpy(race.status, "Too many requests - waiting 30 s");
  draw();
  CHECK(gfx->shows("Too many requests"));
  race.status[0] = 0;

  // loop() runs wifi, data and drawing (at most every 250 ms)
  for (int i = 0; i < 60 && !race.ready; i++)
  {
    loop();
    fake_millis += 100;
  }
  CHECK(race.ready);

  // Hold: restart from the grid
  fake_touch.press(240, 160);
  loop();
  fake_millis += 800;
  loop();
  fake_touch.release();
  fake_millis += 300;
  loop();
  int before = gfx->flushes;
  loop(); // too soon to draw again
  CHECK_EQ(gfx->flushes, before);
  CHECK(gfx->shows("Mexico City - Race (replay x1)"));
  CHECK(gfx->shows("On the grid"));
  CHECK(gfx->shows("P1 PIA"));
  CHECK(gfx->shows("P3 LEC"));
  CHECK(gfx->rects >= 3); // a team-colour bar per podium driver

  // Tap: speed up to 10x
  fake_touch.press(240, 160);
  loop();
  fake_touch.release();
  fake_millis += 300;
  loop();
  CHECK(gfx->shows("(replay x10)"));

  // 10x for 27 s = 4.5 race minutes: lap 2 with the race clock
  fake_millis += 27000;
  loop();
  CHECK(gfx->shows("Lap 2 / 5"));
  CHECK(gfx->shows("Race clock 0:04:3"));
  CHECK(gfx->shows("P1 NOR"));
  CHECK(gfx->shows("P2 PIA"));
  CHECK(gfx->shows("P3 LEC"));

  // P3 can be empty
  f1Driver(16)->pos = 0;
  race.status[0] = 0;
  draw();
  CHECK(!gfx->shows("P3"));
}

int main()
{
  test_parse_iso();
  test_display();
  test_touch_raw();
  test_touch_gestures();
  test_net();
  test_loading();
  test_replay_sync();
  test_race_story();
  test_join_mid_race();
  test_news_ring();
  test_intervals();
  test_locations();
  test_stream_buffer_full();
  test_load_errors();
  test_gzip();
  test_bad_replies();
  test_limits();
  test_more_locations();
  test_checkpoint_sketch();
  printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
