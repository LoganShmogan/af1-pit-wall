// Host unit tests for the DramaBoard sketch.
// Run: make -C test drama
#include "testing.h"

// The sketch under test (compiled from sketch_drama.cpp)
const char *clipFor(const F1News &n);
void setup();
void loop();
extern uint32_t nextSeq;
extern int queued;
extern F1News card;
extern bool cardOn, muted;

static bool sounded(const char *clip) { return fake_serial.find(std::string("SOUND: ") + clip) != std::string::npos; }

static void frame(int ms = 100)
{
  fake_millis += ms;
  loop();
}
static void tap(int x, int y)
{
  fake_touch.press(x, y);
  frame();
  fake_touch.release();
  frame();
}
static void hold(int x, int y)
{
  fake_touch.press(x, y);
  frame();
  frame(800);
  fake_touch.release();
  frame();
}
static void push(F1NewsKind k, uint8_t drv = 0, uint8_t drv2 = 0, uint8_t turn = 0, const char *text = "")
{
  F1News n = {};
  n.kind = k, n.drv = drv, n.drv2 = drv2, n.turn = turn, n.lap = race.lap;
  strlcpy(n.text, text, sizeof(n.text));
  f1PushNews(n);
}
static void addDriver(uint8_t num, const char *code, uint8_t pos, int32_t interval)
{
  F1Driver &d = race.drivers[race.driverCount++];
  d = F1Driver();
  d.num = num, d.pos = pos, d.intervalMs = interval, d.colour = 0xFC00;
  strlcpy(d.code, code, sizeof(d.code));
}

// A fresh board with a race loaded by hand: NOR leads PIA by 1.234 s on lap 12 / 71.
static void start()
{
  resetFakes();
  WiFi.st = WL_DISCONNECTED; // f1Tick stays idle
  setup();
  cardOn = false, queued = 0, muted = false;
  race.ready = true;
  race.lap = 12, race.totalLaps = 71;
  race.track = TRACK_GREEN;
  addDriver(1, "NOR", 1, 0);
  addDriver(81, "PIA", 2, 1234);
  frame(4000); // well clear of any earlier clip
  fake_serial.clear();
}

static void testLoading()
{
  resetFakes();
  WiFi.st = WL_DISCONNECTED;
  setup();
  frame();
  CHECK(gfx->shows("Loading race..."));
  CHECK(!gfx->shows("Lap"));
  strlcpy(race.status, "OpenF1 timeout", sizeof(race.status));
  hold(10, 10); // toys are off while loading
  CHECK(gfx->shows("OpenF1 timeout"));
  CHECK_EQ(race.newsCount, 0u);
}

static void testClips()
{
  F1News n = {};
  const char *want[] = {"lights out away we go.mp3", "chequered flag.mp3", "taken the lead.mp3", "fastest lap.mp3",
                        "diving into the pits.mp3", "yellow flags.mp3", "safety car is out.mp3", "yellow flags.mp3",
                        "safety car coming in.mp3", nullptr, "green flag we are racing.mp3", nullptr};
  for (int k = NEWS_START; k <= NEWS_PENALTY; k++)
  {
    n.kind = (F1NewsKind)k;
    const char *c = clipFor(n);
    CHECK(want[k] ? c && std::string(c) == want[k] : c == nullptr);
  }
  n.kind = NEWS_CRASH;
  n.turn = 4;
  CHECK_STR(clipFor(n), "crash at turn 04.mp3");
  n.turn = 20;
  CHECK_STR(clipFor(n), "crash at turn 20.mp3");
  n.turn = 21;
  CHECK_STR(clipFor(n), "yellow flags.mp3");
}

static void testDashboard()
{
  start();
  race.fastestLapMs = 78456, race.fastestLapDrv = 1, race.fastestLapNo = 43;
  race.fastestPitMs = 2400, race.fastestPitDrv = 81;
  race.hasWeather = true, race.airC = 24.4, race.trackC = 41.2, race.humidity = 45, race.raining = true;
  frame();
  CHECK(gfx->shows("LAP 12 / 71"));
  CHECK(gfx->shows("GREEN"));
  CHECK(gfx->shows("8:05 am"));
  CHECK(gfx->shows("NOR") && gfx->shows("PIA"));
  CHECK(gfx->shows("GAP +1.234"));
  CHECK(gfx->shows("1:18.456") && gfx->shows("LAP 43"));
  CHECK(gfx->shows("2.4 s"));
  CHECK(gfx->shows("AIR 24C") && gfx->shows("TRACK 41C") && gfx->shows("HUMIDITY 45%"));
  CHECK(gfx->shows("RAIN"));
  CHECK(gfx->shows("All quiet so far."));
  tap(300, 60);  // empty feed
  tap(450, 100); // below the speaker
  tap(100, 200); // left panel
  CHECK(!cardOn && !muted);
  int flushes = gfx->flushes;
  frame(0); // too soon to redraw
  CHECK_EQ(gfx->flushes, flushes);

  // The quiet version: before the start, nothing known yet
  race.lap = 0, race.track = TRACK_UNKNOWN, race.driverCount = 0;
  race.fastestLapMs = 0, race.fastestPitMs = 0, race.hasWeather = false;
  fake_local_ok = false;
  frame();
  CHECK(gfx->shows("GRID"));
  CHECK(!gfx->shows("GREEN"));
  CHECK(gfx->shows("---") && gfx->shows("GAP --"));
  CHECK(!gfx->shows(" am") && !gfx->shows(" pm"));

  // P2 known but no interval yet; dry; red flag; afternoon and midnight clock
  fake_local_ok = true;
  addDriver(81, "PIA", 2, -1);
  race.hasWeather = true, race.raining = false;
  race.track = TRACK_RED;
  fake_time = NOV7 + 5 * 3600; // 13:05
  frame();
  CHECK(gfx->shows("GAP --"));
  CHECK(!gfx->shows("RAIN"));
  CHECK(gfx->shows("RED"));
  CHECK(gfx->shows("1:05 pm"));
  fake_time = NOV7 - 8 * 3600; // 00:05
  frame();
  CHECK(gfx->shows("12:05 am"));
}

static void testFeed()
{
  start();
  push(NEWS_PIT, 1, 0, 0, "NOR pits - 2.4 s");
  frame();
  CHECK(gfx->shows("PIT STOP") && gfx->shows("L12") && gfx->shows("NOR pits - 2.4 s"));
  CHECK(!cardOn);                    // pit stops don't get a card...
  CHECK(fake_serial.empty());        // ...or a sound
  push(NEWS_PENALTY, 1, 0, 0, "CAR 1 (NOR) 5 SECOND TIME PENALTY");
  push(NEWS_PENALTY, 1, 0, 0, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  frame();
  CHECK(gfx->shows("CAR 1 (NOR) 5"));            // word-wrapped to 18 columns
  CHECK(!gfx->shows("SECOND"));
  CHECK(gfx->shows("ABCDEFGHIJKLMNOPQR"));       // one long word: cut
  CHECK(!gfx->shows("ABCDEFGHIJKLMNOPQRS"));
  CHECK(fake_serial.empty()); // penalties have no clip
  for (int i = 0; i < 6; i++) push(NEWS_PIT, 1, 0, 0, "later");
  frame();
  CHECK(!gfx->shows("PENALTY")); // only the newest 6
}

static void testSound()
{
  start();
  push(NEWS_START);
  frame();
  CHECK(sounded("lights out away we go.mp3"));
  push(NEWS_GREEN); // 0.1 s later: skipped, never two at once
  frame();
  CHECK(!sounded("green flag"));
  frame(3000);
  push(NEWS_FINISH);
  frame();
  CHECK(sounded("chequered flag.mp3"));

  tap(450, 20); // mute
  CHECK(muted);
  frame(3000);
  push(NEWS_SC);
  frame();
  CHECK(!sounded("safety car is out"));
  tap(10, 300); // close the SC card
  tap(450, 20); // unmute
  CHECK(!muted);
}

static void testCards()
{
  start();
  push(NEWS_CRASH, 1, 81, 4, "Collision between NOR and PIA at turn 4, both cars out of the race and the marshals are on it");
  frame();
  CHECK(cardOn);
  CHECK(sounded("crash at turn 04.mp3"));
  CHECK(gfx->shows("CRASH - TURN 4") && gfx->shows("NOR") && gfx->shows("PIA") && gfx->shows("TAP TO CLOSE"));
  CHECK(gfx->shows("LAP 12"));
  CHECK(!gfx->shows("FASTEST LAP")); // full screen: no dashboard
  frame(8000);
  CHECK(!cardOn);
  CHECK(gfx->shows("FASTEST LAP"));

  // Banner: lead change over the dashboard, gone after 4 s
  push(NEWS_LEAD, 81, 1, 0, "PIA takes the lead");
  frame();
  CHECK(gfx->shows("NEW LEADER") && gfx->shows("PIA takes the lead") && gfx->shows("FASTEST LAP"));
  frame(4000);
  CHECK(!cardOn);

  // Several at once: most important first, one at a time
  push(NEWS_FASTEST, 1);
  push(NEWS_LEAD, 1);
  push(NEWS_SC);
  push(NEWS_RED);
  push(NEWS_CRASH, 99); // unknown car: no panel
  frame();
  CHECK(card.kind == NEWS_CRASH);
  CHECK(gfx->shows("CRASH") && !gfx->shows("TURN"));
  F1NewsKind order[] = {NEWS_RED, NEWS_SC, NEWS_LEAD, NEWS_FASTEST};
  for (F1NewsKind k : order)
  {
    tap(100, 200); // tap closes the card; the next one comes up
    frame();
    CHECK(cardOn && card.kind == k);
  }
  tap(100, 200);
  frame();
  CHECK(!cardOn);

  // Queue is a fixed 8
  for (int i = 0; i < 10; i++) push(NEWS_FASTEST);
  frame();
  CHECK_EQ(queued, 7); // 8 queued, 1 on screen
}

static void testTouch()
{
  start();
  push(NEWS_PIT, 1, 0, 0, "NOR pits");
  push(NEWS_PIT, 81, 0, 0, "PIA pits");
  frame();
  tap(300, 46 + 5); // newest
  CHECK(cardOn && std::string(card.text) == "PIA pits");
  tap(300, 200); // close
  CHECK(!cardOn);
  tap(300, 46 + 45 + 5); // second row
  CHECK(cardOn && std::string(card.text) == "NOR pits");
  tap(300, 200);
  tap(300, 20); // top bar, not the speaker
  CHECK(!cardOn);

  // Toy button: hold the top bar for a test crash on the leader
  hold(100, 20);
  frame();
  CHECK(cardOn && card.kind == NEWS_CRASH && card.drv == 1 && card.turn == 4);
  CHECK(gfx->shows("Toy button"));
  tap(300, 200);
  race.driverCount = 0;
  hold(100, 20);
  frame();
  CHECK(cardOn && card.drv == 0);
  tap(300, 200);
  uint32_t before = race.newsCount;
  hold(100, 200); // hold below the top bar: nothing
  fake_touch.press(100, 200); // swipe: nothing
  frame();
  fake_touch.press(300, 200);
  frame();
  fake_touch.release();
  frame();
  CHECK_EQ(race.newsCount, before);
  CHECK(!cardOn);
}

static void testCatchUp()
{
  start();
  for (int i = 0; i < 40; i++) push(NEWS_PIT);
  frame();
  CHECK_EQ(nextSeq, race.newsCount); // fell behind by more than F1_NEWS_KEEP: skipped ahead, no crash
}

int main()
{
  testLoading();
  testClips();
  testDashboard();
  testFeed();
  testSound();
  testCards();
  testTouch();
  testCatchUp();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
