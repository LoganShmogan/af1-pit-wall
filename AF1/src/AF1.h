// AF1 shared library: one copy of the screen, touch, wifi/clock and race-data
// code, used by the STANDINGS, MAP and DRAMA sketches.
#pragma once
#include <Arduino_GFX_Library.h>

// ---------- Display (480 x 320 landscape, draw then gfx->flush()) ----------
extern Arduino_Canvas *gfx;

// Starts screen, backlight and touch. Returns false if the screen didn't start
// (usually PSRAM not set to "OPI PSRAM").
bool af1Begin();
void af1Backlight(bool on);
uint16_t af1Rgb(uint8_t r, uint8_t g, uint8_t b);
void af1Text(int x, int y, int size, uint16_t colour, const char *s);
void af1Centred(const char *s, int y, int size, uint16_t colour);

// ---------- Touch ----------
enum TouchKind { T_NONE, T_TAP, T_HOLD, T_SWIPE_LEFT, T_SWIPE_RIGHT, T_SWIPE_UP, T_SWIPE_DOWN };
struct TouchEvent { TouchKind kind; int x, y; }; // x, y = where the finger went down

// Call every loop. TAP/SWIPE fire when the finger lifts; HOLD fires once
// after 0.7 s of holding still (no TAP follows it).
TouchEvent af1Touch();
// Raw read: true while a finger is down.
bool af1TouchRaw(int &x, int &y);

// ---------- Wifi + New Zealand clock ----------
void af1NetBegin(const char *hostname); // non-blocking; uses secrets.h
void af1NetTick();                      // call every loop; reconnects by itself
bool af1WifiOk();
bool af1TimeOk();
int64_t af1NowMs();                     // UTC milliseconds since 1970

// ---------- Race data (OpenF1) ----------
// One board = one program, but all three read the same race through this struct.
// Replays are synced to the NTP clock, so boards started at different times still
// show the same race moment (as long as they use the same replay speed).

// Which OpenF1 feeds a board loads. Each costs requests; the free tier allows about
// 30 a minute for the whole house, so only ask for what the board shows.
enum F1Feed : uint16_t
{
  F1_POSITIONS = 1,     // running order (always useful)
  F1_LAPS = 2,          // lap count, lap times, fastest lap
  F1_STINTS = 4,        // tyre compound + laps on the set
  F1_INTERVALS = 8,     // gap to leader / car ahead (streamed)
  F1_PITS = 16,         // pit stops
  F1_RACE_CONTROL = 32, // flags, safety car, incidents, penalties
  F1_WEATHER = 64,      // air/track temperature, rain
  F1_LOCATION = 128,    // car x/y on track (streamed, map board only)
};

enum F1Track : uint8_t { TRACK_UNKNOWN, TRACK_GREEN, TRACK_VSC, TRACK_SC, TRACK_RED, TRACK_CHEQUERED };

struct F1Driver
{
  uint8_t num;
  char code[4];        // "NOR"
  uint16_t colour;     // team colour (RGB565)
  uint8_t pos;         // current position, 0 = unknown
  uint8_t gridPos;     // position at lights out (for "started P5, now P2"), 0 = unknown
  int32_t gapMs;       // gap to leader, -1 = unknown (leader has 0)
  int32_t intervalMs;  // gap to car ahead, -1 = unknown
  uint8_t lapsDown;    // > 0 when lapped: show "+1 LAP" instead of gapMs
  uint8_t lap;         // lap this driver is on, 0 = not started
  int32_t lastLapMs;   // 0 = none yet
  int32_t bestLapMs;   // 0 = none yet
  char compound;       // 'S','M','H','I','W', 0 = unknown
  uint8_t tyreLaps;    // laps on this set = lap - stint start + 1 (OpenF1's own figure is unreliable)
  uint8_t pitStops;
  bool hasLocation;    // x/y valid (F1_LOCATION only)
  int16_t x, y;        // OpenF1 track coordinates, smoothed between samples
};

// Things worth telling people about, in race order. The drama board turns these into cards and
// sound; the map board uses CRASH/SC for markers. Read with f1News().
enum F1NewsKind : uint8_t
{
  NEWS_START,      // lights out
  NEWS_FINISH,     // chequered flag
  NEWS_LEAD,       // drv took the lead (drv2 = who lost it)
  NEWS_FASTEST,    // drv set the fastest lap so far (ms, lap)
  NEWS_PIT,        // drv pitted (ms = stop time, 0 if unknown; lap)
  NEWS_CRASH,      // incident/collision/stopped car (drv, drv2 if two cars, turn if known)
  NEWS_SC,         // safety car deployed
  NEWS_VSC,        // virtual safety car deployed
  NEWS_SC_ENDING,  // (V)SC ending / in this lap
  NEWS_RED,        // red flag
  NEWS_GREEN,      // green flag / racing resumes
  NEWS_PENALTY,    // drv given a penalty (text has the detail)
};

struct F1News
{
  uint32_t seq;        // 0, 1, 2 ... in the order they happened
  int64_t t;           // race clock when it happened (UTC ms)
  F1NewsKind kind;
  uint8_t drv, drv2;   // driver numbers, 0 = none
  uint8_t lap;         // race lap when it happened
  uint8_t turn;        // corner number, 0 = unknown
  int32_t ms;          // pit stop or lap time, 0 = n/a
  char text[80];       // short human text, e.g. "NOR pits - 2.4 s" or the race control message
};

struct F1Race
{
  bool ready;            // everything asked for has loaded and the replay is running
  long sessionKey;
  int circuitKey;        // OpenF1 circuit_key (picks the map)
  char sessionName[24];  // "Race"
  char circuit[28];      // "Mexico City"
  char country[28];
  int64_t startMs, endMs;
  int64_t clockMs;       // race clock (replayed time), UTC ms
  int lap, totalLaps;    // leader's lap; lap 0 = before lights out
  F1Track track;         // track status
  int32_t fastestLapMs;  // 0 = none yet
  uint8_t fastestLapDrv, fastestLapNo;
  int32_t fastestPitMs;  // fastest stop so far, 0 = none
  uint8_t fastestPitDrv;
  bool hasWeather;
  float airC, trackC;
  uint8_t humidity;
  bool raining;
  uint32_t newsCount;    // news items so far; the newest is f1News(newsCount - 1)
  F1Driver drivers[24];
  int driverCount;
  char status[96];       // last problem, "" when all fine
};
extern F1Race race;

#define F1_NEWS_KEEP 32 // f1News() keeps the most recent 32

// Replay start time (UTC, "2026-11-07T02:00:00"). When set, every board holds the grid until then and
// starts the race from the beginning at that moment, so a test run starts together. "" = the replay
// just loops in step with the clock. Set it at build time: -DF1_REPLAY_START=\"...\"
#ifndef F1_REPLAY_START
#define F1_REPLAY_START ""
#endif

// sessionKey 0 = latest finished Race. speed 1 = real time. feeds = F1Feed flags OR'd together.
void f1Begin(long sessionKey, int speed, uint16_t feeds);
void f1Tick();                        // call every loop (cheap; downloads run in the background)
void f1SetSpeed(int speed);           // keeps the current race moment
void f1Restart();                     // back to the session start (this board only)
void f1StartAt(int64_t utcMs);        // start the race from the beginning at this moment (0 = off)
F1Driver *f1DriverAt(int pos);        // nullptr if nobody is in that position
F1Driver *f1Driver(int num);          // by car number, nullptr if unknown
const F1News *f1News(uint32_t seq);   // nullptr if not happened yet or too old
const char *f1TrackName(F1Track t);   // "GREEN", "SAFETY CAR", ...
void f1PushNews(const F1News &n);     // adds a news item (seq is filled in); the drama board's toy buttons use it

// Helpers (exposed for testing)
int64_t af1ParseIso(const char *s);
