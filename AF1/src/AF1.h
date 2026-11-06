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
struct F1Driver
{
  uint8_t num;
  char code[4];    // "NOR"
  uint16_t colour; // team colour
  uint8_t pos;     // current position, 0 = unknown
};

struct F1Race
{
  bool ready;            // session, drivers, laps and positions all loaded
  long sessionKey;
  char sessionName[24];  // "Race"
  char circuit[28];      // "Mexico City"
  char country[28];
  int64_t startMs, endMs;
  int64_t clockMs;       // race clock (replayed time), UTC ms
  int lap, totalLaps;    // lap 0 = before lights out
  F1Driver drivers[24];
  int driverCount;
  char status[96];       // last problem, "" when all fine
};
extern F1Race race;

// sessionKey 0 = latest finished Race. speed 1 = real time.
void f1Begin(long sessionKey, int speed);
void f1Tick();                    // call every loop
void f1SetSpeed(int speed);       // keeps the current race moment
void f1Restart();                 // back to the session start
F1Driver *f1DriverAt(int pos);    // nullptr if nobody is in that position

// Helpers (exposed for testing)
int64_t af1ParseIso(const char *s);
