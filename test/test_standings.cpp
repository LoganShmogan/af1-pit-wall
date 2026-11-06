// Host tests for the StandingsBoard sketch.
// Run: make -C test standings
#include "testing.h"

void setup();
void loop();

// Wifi off so f1Tick stays idle and the race is whatever the test puts in it.
static void start()
{
  resetFakes();
  WiFi.st = WL_DISCONNECTED;
  setup();
}

static void addDriver(int num, const char *code, int pos, int32_t gapMs, char compound, int tyreLaps)
{
  F1Driver &d = race.drivers[race.driverCount++];
  d = F1Driver();
  d.num = num;
  strlcpy(d.code, code, sizeof(d.code));
  d.colour = 0xF800;
  d.pos = pos;
  d.gapMs = gapMs;
  d.intervalMs = -1;
  d.compound = compound;
  d.tyreLaps = tyreLaps;
}

// 20 cars: P1-P20 except P13 is empty, a mix of tyres, gaps and lapped cars.
static void fillRace()
{
  race.ready = true;
  strlcpy(race.circuit, "Mexico City", sizeof(race.circuit));
  strlcpy(race.sessionName, "Race", sizeof(race.sessionName));
  race.lap = 12;
  race.totalLaps = 71;
  const char tyres[] = "SMHIW";
  char code[4];
  for (int p = 1; p <= 21; p++)
  {
    if (p == 13) continue;
    snprintf(code, sizeof(code), "D%02d", p);
    addDriver(p + 1, code, p, (p - 1) * 1234, p == 5 ? 0 : tyres[p % 5], p);
  }
  F1Driver *d = f1DriverAt(1);
  d->gridPos = 3, d->bestLapMs = 83456, d->lastLapMs = 84001, d->pitStops = 1;
  f1DriverAt(2)->gridPos = 1;
  f1DriverAt(3)->gapMs = -1;
  f1DriverAt(20)->lapsDown = 1;
  f1DriverAt(21)->lapsDown = 2;
}

static void tap(int x, int y)
{
  fake_touch.press(x, y);
  loop();
  fake_touch.release();
  loop();
}

static void test_loading()
{
  start();
  loop();
  CHECK(gfx->shows("Loading race..."));
  CHECK(gfx->texts.size() == 2);
  strlcpy(race.status, "No wifi", sizeof(race.status));
  tap(100, 100); // ignored while loading
  CHECK(gfx->shows("No wifi"));
}

static void test_list()
{
  start();
  fillRace();
  loop();
  CHECK(gfx->shows("LAP 12"));
  CHECK(gfx->shows("/ 71"));
  CHECK(gfx->shows("Mexico City - Race"));
  CHECK(gfx->shows("8:05 am"));
  CHECK(gfx->shows("D01"));
  CHECK(gfx->shows("D21"));
  CHECK(gfx->shows("LEADER"));
  CHECK(gfx->shows("+1.2"));
  CHECK(gfx->shows("+11.1")); // P10: 9 x 1234 ms
  CHECK(gfx->shows("+1 LAP"));
  CHECK(gfx->shows("+2 LAPS"));
  CHECK(!gfx->shows("D13"));
  CHECK(!gfx->shows("+2.4")); // P3's gap is unknown

  race.lap = 0;
  fake_time = NOV7 + 12 * 3600; // 8:05 pm
  loop();
  CHECK(gfx->shows("GRID"));
  CHECK(gfx->shows("8:05 pm"));
  fake_time = NOV7 - 8 * 3600; // 0:05 UTC
  loop();
  CHECK(gfx->shows("12:05 am"));
  fake_local_ok = false;
  loop();
  CHECK(!gfx->shows(" am"));
}

static void test_feature()
{
  start();
  fillRace();
  fake_millis += 90000;
  loop();
  CHECK(gfx->shows("TOP 3"));
  CHECK(gfx->shows("P1"));
  CHECK(gfx->shows("D01"));
  CHECK(gfx->shows("1:23.456"));
  CHECK(gfx->shows("1 LAPS"));
  CHECK(!gfx->shows("PIT STOPS"));
  fake_millis += 6000;
  loop();
  CHECK(gfx->shows("D02"));
  CHECK(gfx->shows("-")); // no best lap
  fake_millis += 6000;
  loop();
  CHECK(gfx->shows("D03"));
  fake_millis += 6000;
  loop();
  CHECK(gfx->shows("LAP 12")); // back to the list
  CHECK(!gfx->shows("TOP 3"));

  // Feature with nobody in P2 stops early
  f1DriverAt(2)->pos = 0;
  fake_millis += 96000;
  loop();
  CHECK(gfx->shows("TOP 3"));
  fake_millis += 6000;
  loop();
  CHECK(gfx->shows("LAP 12"));

  // A tap during the feature returns to the list
  fake_millis += 90000;
  loop();
  CHECK(gfx->shows("TOP 3"));
  tap(300, 200);
  CHECK(gfx->shows("LAP 12"));
}

static void test_card()
{
  start();
  fillRace();
  tap(100, 50); // P1 row
  CHECK(gfx->shows("Started P3 - now P1 (+2)"));
  CHECK(gfx->shows("1:24.001"));
  CHECK(gfx->shows("PIT STOPS"));
  tap(100, 50);
  CHECK(gfx->shows("LAP 12"));

  tap(100, 150); // P5: no tyre, no grid position
  CHECK(gfx->shows("Now P5"));
  fake_millis += 8000;
  loop();
  CHECK(gfx->shows("LAP 12")); // timed out

  tap(300, 70); // P13 is empty
  CHECK(gfx->shows("LAP 12"));
  tap(300, 20); // header
  CHECK(gfx->shows("LAP 12"));
  fake_touch.press(300, 319); // below the last row
  loop();
  fake_touch.release();
  loop();
  CHECK(gfx->shows("LAP 12"));

  tap(300, 300); // P22 row is empty too
  tap(300, 280); // P21
  CHECK(gfx->shows("Now P21"));
  race.driverCount = 0; // driver gone: back to the list
  loop();
  CHECK(gfx->shows("LAP 12"));
}

int main()
{
  test_loading();
  test_list();
  test_feature();
  test_card();
  printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
