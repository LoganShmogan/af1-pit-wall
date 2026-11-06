/*
  AF1 - Map board (middle screen): the cars racing on the track map.

  - Race running at a known circuit: that circuit's map with every car on it, podium and
    battle rings, crash ripples, safety-car beacon, running order and lap/time along the bottom.
  - Otherwise: the 23 season maps. Tap right half / swipe left = next, tap left / swipe right = back.
  - Tap a car: its code and position for 3 s.

  Arduino: Tools > Partition Scheme = Custom (uses partitions.csv in this folder, room for the maps),
  plus the usual AF1 settings (docs/hardware.md). Libraries: AF1, GFX Library for Arduino, PNGdec.
  Maps and their calibration come from tools/build_maps.py.
*/
#include <AF1.h>
#include <PNGdec.h>
#include <math.h>
#include <algorithm>
#include "src/maps.h"

const int REPLAY_SPEED = 1;                // same on every board so they stay in sync
const uint32_t FRAME_MS = 50;              // about 20 frames a second
const size_t FB_BYTES = 480 * 320 * 2;     // one full screen of RGB565

PNG png;
uint16_t lineBuf[240];
uint16_t *background;   // copy of the screen with just the map on it
int shownMap = -1;      // map in `background`
int browseMap = 0;      // map picked by tap/swipe when there's no race
uint32_t lastDraw = 0;
char drawnKey[32];      // what the last not-ready frame showed (skip identical frames)
uint32_t seenNews = 0;  // next news seq to look at
F1News crashes[4];      // last few crashes, for the red ripples
int crashCount = 0;
uint8_t labelNum = 0;   // tapped car
uint32_t labelUntil = 0;

const uint16_t DARK = 0x10A2;

// ---------- map background ----------
// Each map is two PNGs, the left and right halves: PNGdec can't buffer a 480-wide colour line.
int pngX; // where the half being decoded goes

int pngLine(PNGDRAW *p)
{
  png.getLineAsRGB565(p, lineBuf, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  gfx->draw16bitRGBBitmap(pngX, p->y, lineBuf, p->iWidth, 1);
  return 1;
}

void decodeHalf(const uint8_t *data, uint32_t size, int x)
{
  // The maps are fixed at build time and the host tests decode all 23, so no error path here.
  pngX = x;
  png.openFLASH((uint8_t *)data, size, pngLine);
  png.decode(nullptr, 0);
  png.close();
}

// Decode the PNGs only when the map changes; every frame starts from the saved copy.
void showBackground(int i)
{
  if (i != shownMap)
  {
    decodeHalf(MAPS[i].left, MAPS[i].leftSize, 0);
    decodeHalf(MAPS[i].right, MAPS[i].rightSize, 240);
    memcpy(background, gfx->getFramebuffer(), FB_BYTES);
    shownMap = i;
  }
  memcpy(gfx->getFramebuffer(), background, FB_BYTES);
}

// Map for the race being shown, -1 if no race or no map for that circuit.
int raceMap()
{
  if (!race.ready) return -1;
  for (int i = 0; i < MAP_COUNT; i++)
    if (MAPS[i].circuitKey == race.circuitKey) return i;
  return -1;
}

void toScreen(const MapInfo &m, const F1Driver *d, int &x, int &y)
{
  x = lroundf(m.ax * d->x + m.bx * d->y + m.cx);
  y = lroundf(m.ay * d->x + m.by * d->y + m.cy);
}

// ---------- the race on the map ----------
uint16_t tyreColour(char c)
{
  switch (c)
  {
  case 'S': return RGB565_RED;
  case 'M': return RGB565_YELLOW;
  case 'H': return RGB565_WHITE;
  case 'I': return RGB565_GREEN;
  case 'W': return af1Rgb(0, 120, 255);
  default: return 0;
  }
}

bool isBright(uint16_t c) { return (c >> 11) * 2 + ((c >> 5) & 63) + (c & 31) > 70; }

void drawCar(const MapInfo &m, const F1Driver *d)
{
  int x, y;
  toScreen(m, d, x, y);
  gfx->fillCircle(x, y, 7, d->colour);
  gfx->drawCircle(x, y, 7, DARK);
  char n[4];
  snprintf(n, sizeof(n), "%d", d->num);
  af1Text(x - (int)strlen(n) * 3 + 1, y - 3, 1, isBright(d->colour) ? RGB565_BLACK : RGB565_WHITE, n);
  if (d->compound)
  {
    gfx->fillCircle(x + 7, y - 7, 3, tyreColour(d->compound));
    gfx->drawCircle(x + 7, y - 7, 3, DARK);
  }
}

// A 2 px ring around a car.
void ring(const MapInfo &m, const F1Driver *d, int r, uint16_t c)
{
  int x, y;
  toScreen(m, d, x, y);
  gfx->drawCircle(x, y, r, c);
  gfx->drawCircle(x, y, r + 1, c);
}

// P1 gold always; P2 silver / P3 blue when within 2 s of the car ahead. Faster pulse = closer.
void podiumRings(const MapInfo &m)
{
  const uint16_t colours[] = {af1Rgb(255, 200, 0), af1Rgb(200, 200, 215), af1Rgb(0, 150, 255)};
  for (int p = 1; p <= 3; p++)
  {
    F1Driver *d = f1DriverAt(p);
    if (!d || !d->hasLocation) continue;
    int gap = p == 1 ? 2000 : d->intervalMs;
    if (gap < 0 || gap > 2000) continue;
    uint32_t period = 400 + gap * 11 / 20; // ms: 1.5 s apart -> slow, touching -> fast
    float wave = sinf(millis() % period * 6.2832f / period + p * 2.1f); // p: out of step with each other
    ring(m, d, 11 + lroundf(wave * 2), colours[p - 1]);
  }
}

// Up to 4 closest fights (under 1 s) behind the podium, flashing in team colours.
void battleRings(const MapInfo &m)
{
  F1Driver *fights[24];
  int n = 0;
  for (int p = 4; p <= race.driverCount; p++)
  {
    F1Driver *d = f1DriverAt(p), *ahead = f1DriverAt(p - 1);
    if (d && ahead && d->hasLocation && ahead->hasLocation && d->intervalMs >= 0 && d->intervalMs < 1000)
      fights[n++] = d;
  }
  std::sort(fights, fights + n, [](F1Driver *a, F1Driver *b) { return a->intervalMs < b->intervalMs; });
  for (int i = 0; i < std::min(n, 4); i++)
  {
    F1Driver *d = fights[i], *ahead = f1DriverAt(d->pos - 1);
    uint32_t flash = 120 + d->intervalMs / 4; // ms on, ms off
    if (millis() / flash % 2) continue;
    ring(m, d, 10, d->colour);
    ring(m, ahead, 10, ahead->colour);
  }
}

// Red rings spreading out from a crashed car for 10 s of race time.
void crashRipples(const MapInfo &m)
{
  for (const F1News &c : crashes)
  {
    int64_t age = race.clockMs - c.t;
    if (age < 0 || age >= 10000) continue;
    for (uint8_t num : {c.drv, c.drv2})
    {
      F1Driver *d = f1Driver(num);
      if (!d || !d->hasLocation) continue;
      int x, y;
      toScreen(m, d, x, y);
      for (int k = 0; k < 3; k++)
        gfx->drawCircle(x, y, 8 + (age + k * 333) % 1000 * 22 / 1000, RGB565_RED);
    }
  }
}

void tapLabel(const MapInfo &m)
{
  F1Driver *d = f1Driver(labelNum);
  if (millis() >= labelUntil || !d || !d->hasLocation) return;
  int x, y;
  toScreen(m, d, x, y);
  char s[12];
  snprintf(s, sizeof(s), "%s P%d", d->code, d->pos);
  int w = (int)strlen(s) * 12 + 8, lx = constrain(x - w / 2, 0, 480 - w), ly = y > 60 ? y - 34 : y + 14;
  gfx->fillRoundRect(lx, ly, w, 22, 4, DARK);
  gfx->drawRoundRect(lx, ly, w, 22, 4, d->colour);
  af1Text(lx + 4, ly + 3, 2, RGB565_WHITE, s);
}

void drawRace(const MapInfo &m)
{
  crashRipples(m);
  podiumRings(m);
  battleRings(m);
  for (int i = 0; i < race.driverCount; i++)
    if (race.drivers[i].hasLocation) drawCar(m, &race.drivers[i]);
  tapLabel(m);
}

// ---------- flags and info ----------
void drawTrackStatus()
{
  bool strobe = millis() / 250 % 2;
  if (race.track == TRACK_SC || race.track == TRACK_VSC)
  {
    const char *s = race.track == TRACK_SC ? "SC" : "VSC";
    gfx->drawRect(0, 0, 480, 320, RGB565_YELLOW);
    gfx->drawRect(1, 1, 478, 318, RGB565_YELLOW);
    gfx->fillRoundRect(4, 4, 58, 30, 6, strobe ? RGB565_YELLOW : RGB565_BLACK);
    af1Text(33 - (int)strlen(s) * 9, 8, 3, strobe ? RGB565_BLACK : RGB565_YELLOW, s);
  }
  else if (race.track == TRACK_RED)
  {
    gfx->fillRect(0, 0, 480, 36, RGB565_RED);
    af1Centred("RED FLAG", 6, 3, RGB565_WHITE);
  }
}

// Running order in two rows (1-11, 12-22) and the lap count, over the map's legend.
void drawOrder()
{
  gfx->fillRect(0, 293, 480, 27, DARK);
  for (int p = 1; p <= 22; p++)
  {
    F1Driver *d = f1DriverAt(p);
    if (!d) continue;
    int x = 2 + (p - 1) % 11 * 34, y = p <= 11 ? 296 : 309;
    char n[4];
    snprintf(n, sizeof(n), "%d", p);
    af1Text(x, y, 1, RGB565_DARKGREY, n);
    af1Text(x + 13, y, 1, d->colour, d->code);
  }
  char s[16];
  snprintf(s, sizeof(s), "LAP %d/%d", race.lap, race.totalLaps);
  af1Text(476 - (int)strlen(s) * 6, 294, 1, RGB565_WHITE, s);
}

// NZ time, e.g. "7:05 pm" ("" until the clock is set).
void clockText(char *s, size_t n)
{
  struct tm t;
  s[0] = 0;
  if (getLocalTime(&t, 0))
    snprintf(s, n, "%d:%02d %s", t.tm_hour % 12 ? t.tm_hour % 12 : 12, t.tm_min, t.tm_hour < 12 ? "am" : "pm");
}

// ---------- input ----------
void onTouch(const TouchEvent &e)
{
  int rm = raceMap();
  if (rm < 0)
  {
    if (e.kind == T_SWIPE_LEFT || (e.kind == T_TAP && e.x >= 240)) browseMap = (browseMap + 1) % MAP_COUNT;
    else if (e.kind == T_SWIPE_RIGHT || e.kind == T_TAP) browseMap = (browseMap + MAP_COUNT - 1) % MAP_COUNT;
    return;
  }
  if (e.kind != T_TAP) return;
  int best = 20 * 20; // must tap within 20 px of a car
  for (int i = 0; i < race.driverCount; i++)
  {
    F1Driver *d = &race.drivers[i];
    if (!d->hasLocation) continue;
    int x, y;
    toScreen(MAPS[rm], d, x, y);
    int dist = (x - e.x) * (x - e.x) + (y - e.y) * (y - e.y);
    if (dist < best)
    {
      best = dist;
      labelNum = d->num;
      labelUntil = millis() + 3000;
    }
  }
}

void readNews()
{
  for (; seenNews < race.newsCount; seenNews++)
  {
    const F1News *n = f1News(seenNews);
    if (n && n->kind == NEWS_CRASH) crashes[crashCount++ % 4] = *n;
  }
}

// ---------- main ----------
void setup()
{
  Serial.begin(115200);
  af1Begin();
  background = (uint16_t *)ps_malloc(FB_BYTES);
  af1NetBegin("af1-map");
  f1Begin(0, REPLAY_SPEED, F1_POSITIONS | F1_LAPS | F1_STINTS | F1_INTERVALS | F1_RACE_CONTROL | F1_LOCATION);
}

void loop()
{
  af1NetTick();
  f1Tick();
  TouchEvent e = af1Touch();
  if (e.kind != T_NONE) onTouch(e);
  readNews();
  if (millis() - lastDraw < FRAME_MS) return;

  int rm = raceMap(), shown = rm < 0 ? browseMap : rm;
  char clock[12], key[32];
  clockText(clock, sizeof(clock));
  snprintf(key, sizeof(key), "%d %d %s", shown, race.ready, clock);
  if (!race.ready && !strcmp(key, drawnKey)) return; // nothing moves until the race loads
  strcpy(drawnKey, key);
  lastDraw = millis();

  showBackground(shown);
  if (rm >= 0) drawRace(MAPS[rm]);
  if (race.ready)
  {
    drawTrackStatus();
    drawOrder();
  }
  af1Text(476 - (int)strlen(clock) * 12, 304, 2, RGB565_WHITE, clock);
  gfx->flush();
}
