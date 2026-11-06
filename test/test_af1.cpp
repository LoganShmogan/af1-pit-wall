// Host unit tests for the AF1 library and the CheckpointA sketch.
// Run: make -C test         (tests + 100% line-coverage gate)
#include <AF1.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ---------- fake state (declared in test/fakes) ----------
uint32_t fake_millis = 0;
time_t fake_time = 0;
std::map<int, int> fake_pins;
std::string fake_serial;
FakeSerial Serial;
FakeTouchChip fake_touch;
TwoWire Wire;
FakeWiFi WiFi;
bool fake_gfx_begin_ok = true;
std::vector<std::pair<std::string, FakeReply>> fake_routes;
std::vector<std::string> fake_requests;
bool fake_http_begin_ok = true;
int fake_tz_calls = 0;
bool fake_local_ok = true;

void configTzTime(const char *, const char *, const char *, const char *) { fake_tz_calls++; }
bool getLocalTime(struct tm *info, uint32_t)
{
  if (!fake_local_ok) return false;
  gmtime_r(&fake_time, info); // tests use UTC so expectations don't depend on the host's zone
  return true;
}
// Linked with -Wl,--wrap=time,--wrap=gettimeofday so the code under test sees fake_time.
extern "C" time_t __wrap_time(time_t *t)
{
  if (t) *t = fake_time;
  return fake_time;
}
extern "C" int __wrap_gettimeofday(struct timeval *tv, void *)
{
  tv->tv_sec = fake_time;
  tv->tv_usec = 250000;
  return 0;
}

// The sketch under test (compiled from checkpoint.cpp)
extern const char *BOARD_NAME;
void draw();
void setup();
void loop();

// ---------- tiny test runner ----------
static int checks = 0, failures = 0;
#define CHECK(cond)                                                        \
  do                                                                       \
  {                                                                        \
    checks++;                                                              \
    if (!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)
#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_STR(a, b) CHECK(std::string(a) == std::string(b))

static const time_t NOV7 = 1794038709; // Sat 7 Nov 2026 08:05:09 UTC

static void resetFakes()
{
  fake_millis = 1000;
  fake_time = NOV7;
  fake_serial.clear();
  fake_routes.clear();
  fake_requests.clear();
  fake_http_begin_ok = true;
  fake_touch = FakeTouchChip();
  WiFi.st = WL_CONNECTED;
  fake_local_ok = true;
}

static void route(const std::string &match, int code, const std::string &body)
{
  fake_routes.push_back({match, {code, body}});
}

// A small race: session 20:00-22:00, lights out 20:03:00, laps every 90 s, lap 3 missing from the data.
static const char *SESSIONS =
    "[{\"session_key\":1,\"date_start\":\"2026-10-26T20:00:00+00:00\",\"date_end\":\"2026-10-26T22:00:00+00:00\","
    "\"session_name\":\"Race\",\"circuit_short_name\":\"Austin\",\"country_name\":\"United States\"},"
    "{\"session_key\":11404,\"date_start\":\"2026-11-01T20:00:00+00:00\",\"date_end\":\"2026-11-01T22:00:00+00:00\","
    "\"session_name\":\"Race\",\"circuit_short_name\":\"Mexico City\",\"country_name\":\"Mexico\",\"gmt_offset\":\"-06:00:00\"},"
    "{\"session_key\":2,\"date_start\":\"2026-10-19T20:00:00+00:00\",\"date_end\":\"2026-10-19T22:00:00+00:00\","
    "\"session_name\":\"Race\",\"circuit_short_name\":\"Older\",\"country_name\":\"X\"}]";
static const char *DRIVERS =
    "[{\"driver_number\":1,\"name_acronym\":\"NOR\",\"team_colour\":\"FF8000\",\"headshot_url\":null},"
    "{\"driver_number\":81,\"name_acronym\":\"PIA\",\"team_colour\":\"FF8000\"},"
    "{\"driver_number\":16,\"name_acronym\":\"LEC\",\"team_colour\":\"E80020\"},"
    "{\"driver_number\":16,\"name_acronym\":\"DUP\",\"team_colour\":\"000000\"},"
    "{\"driver_number\":0,\"name_acronym\":\"BAD\"},{\"driver_number\":100,\"name_acronym\":\"BAD\"},"
    "{\"driver_number\":41}]";
static const char *LAPS =
    "[{\"driver_number\":1,\"lap_number\":1,\"date_start\":\"2026-11-01T20:03:01+00:00\",\"segments_sector_1\":[2049,2051]},"
    "{\"driver_number\":81,\"lap_number\":1,\"date_start\":\"2026-11-01T20:03:00+00:00\"},"
    "{\"driver_number\":16,\"lap_number\":1,\"date_start\":\"2026-11-01T20:03:02+00:00\"},"
    "{\"driver_number\":1,\"lap_number\":2,\"date_start\":\"2026-11-01T20:04:30+00:00\"},"
    "{\"driver_number\":1,\"lap_number\":4,\"date_start\":\"2026-11-01T20:07:30+00:00\"},"
    "{\"driver_number\":1,\"lap_number\":2,\"date_start\":null},"
    "{\"driver_number\":1,\"lap_number\":0,\"date_start\":\"2026-11-01T20:00:00+00:00\"},"
    "{\"driver_number\":1,\"lap_number\":101,\"date_start\":\"2026-11-01T20:00:00+00:00\"}]";
static const char *POSITIONS =
    "[{\"date\":\"2026-11-01T20:04:00+00:00\",\"driver_number\":1,\"position\":1},"   // NOR passes PIA on lap 1
    "{\"date\":\"2026-11-01T20:04:00+00:00\",\"driver_number\":81,\"position\":2},"
    "{\"date\":\"2026-11-01T19:10:00+00:00\",\"driver_number\":81,\"position\":1},"   // grid (out of order)
    "{\"date\":\"2026-11-01T19:10:00+00:00\",\"driver_number\":1,\"position\":2},"
    "{\"date\":\"2026-11-01T19:10:00+00:00\",\"driver_number\":16,\"position\":3},"
    "{\"date\":\"2026-11-01T20:30:00+00:00\",\"driver_number\":99,\"position\":4},"   // driver not in the list
    "{\"date\":\"2026-11-01T22:20:00+00:00\",\"driver_number\":16,\"position\":2},"   // after the scheduled end
    "{\"date\":\"\",\"driver_number\":1,\"position\":1},"
    "{\"date\":\"2026-11-01T20:05:00+00:00\",\"driver_number\":0,\"position\":1},"
    "{\"date\":\"2026-11-01T20:05:00+00:00\",\"driver_number\":100,\"position\":1},"
    "{\"date\":\"2026-11-01T20:05:00+00:00\",\"driver_number\":1,\"position\":0},"
    "{\"date\":\"2026-11-01T20:05:00+00:00\",\"driver_number\":1,\"position\":31}]";

static const int64_t START = 1793563200000;      // 2026-11-01T20:00:00Z
static const int64_t END_PLANNED = 1793570400000; // 22:00:00Z
static const int64_t LAST_EVENT = 1793571600000;  // 22:20:00Z

static void routeRace()
{
  route("sessions?", 200, SESSIONS);
  route("drivers?", 200, DRIVERS);
  route("laps?", 200, LAPS);
  route("position?", 200, POSITIONS);
}

// Tick the data layer through all the loading steps (requests are spaced 600 ms apart).
static void loadAll()
{
  for (int i = 0; i < 6 && !race.ready; i++)
  {
    f1Tick();
    fake_millis += 600;
  }
}

static F1Driver *driver(int num)
{
  for (int i = 0; i < race.driverCount; i++)
    if (race.drivers[i].num == num) return &race.drivers[i];
  return nullptr;
}

// ---------- tests ----------
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
  CHECK_EQ(af1NowMs(), (int64_t)NOV7 * 1000 + 250);
}

static void test_replay_happy_path()
{
  resetFakes();
  routeRace();
  f1Begin(0, 1);
  CHECK(!race.ready);

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
  CHECK_EQ(fake_requests.size(), 4u);
  CHECK_STR(race.status, "");

  // Session: the most recent of the three
  CHECK_EQ(race.sessionKey, 11404);
  CHECK_STR(race.circuit, "Mexico City");
  CHECK_STR(race.country, "Mexico");
  CHECK_STR(race.sessionName, "Race");
  CHECK_EQ(race.startMs, START);
  CHECK_EQ(race.endMs, LAST_EVENT); // stretched to the last position change

  // Drivers: duplicates and bad numbers dropped, missing fields defaulted
  CHECK_EQ(race.driverCount, 4);
  CHECK_STR(driver(1)->code, "NOR");
  CHECK_EQ(driver(1)->colour, af1Rgb(0xFF, 0x80, 0x00));
  CHECK_STR(driver(16)->code, "LEC");
  CHECK_STR(driver(41)->code, "???");
  CHECK_EQ(driver(41)->colour, 0xFFFF);

  // Laps: highest valid lap number
  CHECK_EQ(race.totalLaps, 4);
  f1Restart(); // loadAll() ticked on past the load; start the replay clock from here

  // At the session start: grid order applied, lap 0
  f1Tick();
  CHECK_EQ(race.clockMs, START);
  CHECK_EQ(race.lap, 0);
  CHECK_STR(f1DriverAt(1)->code, "PIA");
  CHECK_STR(f1DriverAt(2)->code, "NOR");
  CHECK(f1DriverAt(4) == nullptr);
  CHECK_EQ(driver(41)->pos, 0);

  // 20:03:00 = lap 1 starts (earliest of the three lap-1 times)
  fake_millis += 180000;
  f1Tick();
  CHECK_EQ(race.lap, 1);
  CHECK_STR(f1DriverAt(1)->code, "PIA");
  // 20:04:00 = NOR takes the lead
  fake_millis += 60000;
  f1Tick();
  CHECK_STR(f1DriverAt(1)->code, "NOR");
  CHECK_EQ(race.lap, 1);
  // 20:06:00 = lap 3 has no data, so it still reads lap 2
  fake_millis += 120000;
  f1Tick();
  CHECK_EQ(race.lap, 2);

  // 60x: 2 s of wall time = 2 race minutes -> 20:08:00, lap 4
  f1SetSpeed(60);
  fake_millis += 2000;
  f1Tick();
  CHECK_EQ(race.clockMs, START + 8 * 60000);
  CHECK_EQ(race.lap, 4);

  // Runs off the end: clock stops at the last event
  fake_millis += 3600000;
  f1Tick();
  CHECK_EQ(race.clockMs, LAST_EVENT);
  CHECK_EQ(driver(16)->pos, 2);

  // Restart: back to the grid
  f1Restart();
  f1Tick();
  CHECK_EQ(race.clockMs, START);
  CHECK_STR(f1DriverAt(1)->code, "PIA");
  CHECK_EQ(driver(16)->pos, 3);
  CHECK_EQ(fake_requests.size(), 4u); // playing makes no more requests
}

static void test_replay_by_session_key()
{
  resetFakes();
  routeRace();
  f1Begin(11404, 1);
  f1Tick();
  CHECK(fake_requests[0].find("sessions?session_key=11404") != std::string::npos);
}

static void expectFailure(const char *endpoint, int code, const char *body, const char *status, uint32_t retryMs)
{
  resetFakes();
  route(endpoint, code, body);
  routeRace();
  f1Begin(0, 1);
  for (int i = 0; i < 6 && !race.status[0]; i++) // stop right after the failing request
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

static void test_replay_errors()
{
  expectFailure("sessions?", 401, "", "refused sessions (401)", 60000);
  expectFailure("sessions?", 403, "", "refused sessions (403)", 60000);
  expectFailure("drivers?", 429, "", "Too many requests", 30000);
  expectFailure("laps?", 404, "", "No laps data", 10000);
  expectFailure("position?", 500, "", "OpenF1 error 500 loading positions", 10000);
  expectFailure("sessions?", 200, "[]", "No session data", 10000);
  expectFailure("sessions?", 200, "{\"detail\":\"oops\"}", "No session data", 10000);
  expectFailure("sessions?", 200, "[{\"session_key\":}]", "No session data", 10000); // broken JSON

  // Nothing answers at all
  resetFakes();
  f1Begin(0, 1);
  f1Tick();
  CHECK(strstr(race.status, "Can't reach OpenF1 (connection refused)") != nullptr);
  resetFakes();
  routeRace();
  fake_http_begin_ok = false;
  f1Begin(0, 1);
  f1Tick();
  CHECK(strstr(race.status, "Can't reach OpenF1") != nullptr);
}

static void test_replay_limits()
{
  // More than 24 drivers and more than 4000 position changes: extra ones are dropped
  std::string drivers = "[", positions = "[";
  for (int i = 1; i <= 30; i++) drivers += (i > 1 ? "," : "") + std::string("{\"driver_number\":") + std::to_string(i) + "}";
  for (int i = 0; i < 4100; i++)
    positions += (i ? "," : "") + std::string("{\"date\":\"2026-11-01T20:10:00+00:00\",\"driver_number\":1,\"position\":") +
                 std::to_string(i % 20 + 1) + "}";
  resetFakes();
  route("drivers?", 200, drivers + "]");
  route("position?", 200, positions + "]");
  routeRace();
  f1Begin(0, 1);
  loadAll();
  CHECK(race.ready);
  CHECK_EQ(race.driverCount, 24);
  CHECK(fake_serial.find("4000 position changes") != std::string::npos);
  CHECK_EQ(race.endMs, END_PLANNED); // nothing after the scheduled end this time

  // A session with no position data still plays (lap count only)
  resetFakes();
  route("position?", 200, "[]");
  routeRace();
  f1Begin(0, 1);
  loadAll();
  CHECK(race.ready);
  CHECK_EQ(race.endMs, END_PLANNED);
  CHECK(f1DriverAt(1) == nullptr);
}

// ---------- CheckpointA sketch ----------
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
  int before = gfx->flushes;
  fake_millis += 300;
  loop();
  loop(); // too soon to draw again
  CHECK_EQ(gfx->flushes, before + 1);
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

  // 20:04:30 at 10x, then lap 2 shows with the race clock
  fake_millis += 27000;
  loop();
  CHECK(gfx->shows("Lap 2 / 4"));
  CHECK(gfx->shows("Race clock 0:0"));
  CHECK(gfx->shows("P1 NOR"));
  CHECK(gfx->shows("P2 PIA"));
  CHECK(gfx->shows("P3 LEC"));

  // Hold: restart
  fake_touch.press(240, 160);
  loop();
  fake_millis += 800;
  loop();
  fake_touch.release();
  fake_millis += 300;
  loop();
  CHECK(gfx->shows("On the grid"));
}

int main()
{
  test_parse_iso();
  test_display();
  test_touch_raw();
  test_touch_gestures();
  test_net();
  test_replay_happy_path();
  test_replay_by_session_key();
  test_replay_errors();
  test_replay_limits();
  test_checkpoint_sketch();
  printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
