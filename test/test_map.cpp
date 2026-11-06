// Host unit tests for the MapBoard sketch.
// Run: make -C test map
#include "testing.h"
#include "../MapBoard/src/maps.h"
#include <math.h>

// The sketch under test (compiled from sketch_map.cpp)
void setup();
void loop();
extern int shownMap, browseMap;

// Next frame: 50 ms later, with this frame's text only.
static void frame(uint32_t ms = 50)
{
  fake_millis += ms;
  gfx->texts.clear();
  loop();
}

static void tap(int x, int y)
{
  fake_touch.press(x, y);
  loop();
  fake_touch.release();
  frame(60);
}

static void swipe(int x0, int y0, int x1, int y1)
{
  fake_touch.press(x0, y0);
  loop();
  fake_touch.press(x1, y1);
  loop();
  fake_touch.release();
  frame(60);
}

static void hold(int x, int y)
{
  fake_touch.press(x, y);
  loop();
  fake_millis += 800;
  loop();
  fake_touch.release();
  frame();
}

// Put a car at screen pixel (sx, sy) by inverting the map's transform.
static void place(F1Driver &d, const MapInfo &m, int sx, int sy)
{
  float det = m.ax * m.by - m.bx * m.ay, X = sx - m.cx, Y = sy - m.cy;
  d.x = lroundf((m.by * X - m.bx * Y) / det);
  d.y = lroundf((m.ax * Y - m.ay * X) / det);
  d.hasLocation = true;
}

static void news(F1NewsKind kind, int drv, int drv2, int64_t t)
{
  F1News n = {};
  n.kind = kind;
  n.drv = drv;
  n.drv2 = drv2;
  n.t = t;
  f1PushNews(n);
}

static void start()
{
  resetFakes();
  WiFi.st = WL_DISCONNECTED; // f1Tick stays idle, the tests fill `race` by hand
  setup();
}

// ---------- no race: browse the 23 maps ----------
static void test_browse()
{
  start();
  int bitmaps = gfx->bitmaps, flushes = gfx->flushes;
  frame();
  CHECK_EQ(shownMap, 0);
  CHECK_EQ(gfx->bitmaps - bitmaps, 640); // two half-width PNG lines per screen row
  CHECK_EQ(gfx->flushes - flushes, 1);
  CHECK(gfx->shows("8:05 am"));
  CHECK(!gfx->shows("LAP"));

  frame(); // nothing changed: no redraw
  CHECK_EQ(gfx->flushes - flushes, 1);
  fake_time += 60;
  frame(); // the clock moved: redraw, but the map is not decoded again
  CHECK(gfx->shows("8:06 am"));
  CHECK_EQ(gfx->flushes - flushes, 2);
  CHECK_EQ(gfx->bitmaps - bitmaps, 640);
  fake_millis += 10;
  fake_time += 60;
  loop(); // under 50 ms since the last frame
  CHECK_EQ(gfx->flushes - flushes, 2);

  // Every map decodes; tapping the right half 23 times comes back round to round 1
  for (int i = 1; i <= 23; i++)
  {
    tap(400, 150);
    CHECK_EQ(browseMap, i % 23);
    CHECK_EQ(shownMap, i % 23);
  }
  CHECK_EQ(gfx->bitmaps - bitmaps, 24 * 640);
  tap(100, 150);
  CHECK_EQ(browseMap, 22);
  swipe(300, 150, 100, 150);
  CHECK_EQ(browseMap, 0);
  swipe(100, 150, 300, 150);
  CHECK_EQ(browseMap, 22);
  swipe(200, 250, 200, 50); // up: nothing
  hold(200, 150);           // hold: nothing
  CHECK_EQ(browseMap, 22);

  fake_time = NOV7 + 4 * 3600; // 12:05 pm
  frame();
  CHECK(gfx->shows("12:05 pm"));
  fake_local_ok = false;
  frame();
  CHECK(!gfx->shows(":"));
}

// ---------- a race at Albert Park (round 1) ----------
static const char *CODES[] = {"NOR", "PIA", "LEC", "HAM", "VER", "RUS", "ANT", "ALB", "SAI", "GAS", "OCO", "BEA", "HUL", "STR"};

static void setupRace()
{
  race.ready = true;
  race.circuitKey = 10;
  race.lap = 12;
  race.totalLaps = 56;
  race.clockMs = 1800000000000;
  race.track = TRACK_GREEN;
  race.driverCount = 14;
  const MapInfo &m = MAPS[0];
  const char tyres[] = "SMHIWX";
  const int gaps[] = {0, 1500, 2500, 300, 1200, -1, 100, 500, 900, 700, 0, 200, 0, 0};
  for (int i = 0; i < 14; i++)
  {
    F1Driver &d = race.drivers[i];
    d = F1Driver();
    d.num = i + 1;
    strcpy(d.code, CODES[i]);
    d.colour = i % 2 ? af1Rgb(0xFF, 0x80, 0x00) : af1Rgb(0x10, 0x10, 0x80);
    d.pos = i + 1;
    d.compound = i < 6 ? tyres[i] : 0;
    d.intervalMs = gaps[i];
    place(d, m, 60 + i * 15, 150);
  }
  race.drivers[10].hasLocation = false; // P11: no location (so P12's fight has no car ahead on the map)
  race.drivers[12].pos = 0;             // nobody at P13
  place(race.drivers[11], m, 300, 40);  // P12 near the top: its label goes below
}

static void test_race()
{
  start();
  setupRace();
  int circles = gfx->circles;
  frame();
  CHECK_EQ(shownMap, 0);
  CHECK(gfx->shows("LAP 12/56"));
  CHECK(gfx->shows("NOR"));
  CHECK(gfx->shows("STR")); // P14 in the second row
  CHECK(gfx->shows("14"));  // car numbers
  CHECK(gfx->circles - circles > 26);

  // Battle rings flash on and off; the race draws every 50 ms
  int flushes = gfx->flushes;
  for (int i = 0; i < 12; i++) frame();
  CHECK_EQ(gfx->flushes - flushes, 12);

  // Podium edge cases: no P1, P2 gap unknown, P1 without a location
  race.drivers[0].pos = 0;
  race.drivers[1].intervalMs = -1;
  frame();
  race.drivers[0].pos = 1;
  race.drivers[0].hasLocation = false;
  frame();
  race.drivers[0].hasLocation = true;

  for (int i = 3; i < 14; i++) race.drivers[i].intervalMs = 5000; // battles off, so circle counts are steady

  // Crashes: 40 old items first (the oldest have dropped out of f1News)
  for (int i = 0; i < 40; i++) news(NEWS_PIT, 1, 0, race.clockMs);
  news(NEWS_CRASH, 1, 0, race.clockMs - 1000);   // P1, no second car
  news(NEWS_CRASH, 11, 2, race.clockMs - 5000);  // P11 has no location, P2 does
  news(NEWS_CRASH, 3, 0, race.clockMs + 5000);   // not yet
  news(NEWS_CRASH, 4, 0, race.clockMs - 20000);  // long over
  circles = gfx->circles;
  frame();
  int withCrash = gfx->circles - circles;
  race.clockMs += 30000;
  circles = gfx->circles;
  frame();
  CHECK_EQ(withCrash - (gfx->circles - circles), 6); // 3 rings each on P1 and P2

  // Safety car, VSC (strobing), red flag
  race.track = TRACK_SC;
  frame();
  CHECK(gfx->shows("SC"));
  frame(250);
  CHECK(gfx->shows("SC"));
  race.track = TRACK_VSC;
  frame();
  CHECK(gfx->shows("VSC"));
  race.track = TRACK_RED;
  frame();
  CHECK(gfx->shows("RED FLAG"));
  race.track = TRACK_GREEN;
  frame();
  CHECK(!gfx->shows("FLAG"));
}

static void test_taps()
{
  start();
  setupRace();
  frame();
  tap(62, 150); // nearest is P1 (P2 is 15 px away)
  CHECK(gfx->shows("NOR P1"));
  frame(3000);
  CHECK(!gfx->shows("NOR P1"));
  tap(300, 42); // P12 near the top
  CHECK(gfx->shows("BEA P12"));
  tap(400, 250); // nothing there: the label stays
  CHECK(gfx->shows("BEA P12"));
  race.drivers[11].hasLocation = false;
  frame();
  CHECK(!gfx->shows("BEA P12"));
  race.drivers[11].hasLocation = true;
  race.drivers[11].num = 99; // driver gone
  frame();
  CHECK(!gfx->shows("BEA P12"));
  swipe(300, 150, 100, 150); // swipes don't flip the map during a race
  CHECK_EQ(shownMap, 0);

  // Circuit without a map: browse the maps, no cars, but the running order still shows
  race.circuitKey = 999;
  int circles = gfx->circles;
  frame();
  CHECK_EQ(shownMap, browseMap);
  CHECK(gfx->shows("LAP 12/56"));
  CHECK_EQ(gfx->circles, circles);
  int before = browseMap;
  tap(400, 150);
  CHECK_EQ(shownMap, (before + 1) % 23);
}

int main()
{
  test_browse();
  test_race();
  test_taps();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
