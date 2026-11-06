/*
  AF1 - Standings board (left screen).
  Running order with gaps and tyres; every 90 s a feature on the top 3; tap a driver for their card.

  Touch:
    tap a driver row = driver card (tap anywhere or wait 8 s to go back)
    tap during the top-3 feature = back to the list
*/
#include <AF1.h>

const long SESSION_KEY = 0; // 0 = latest finished race, or an OpenF1 session_key
const int REPLAY_SPEED = 1; // keep 1 on every board so the three screens stay in sync

const uint32_t FEATURE_EVERY_MS = 90000; // list time between top-3 features
const uint32_t FEATURE_EACH_MS = 6000;   // per driver in the feature
const uint32_t CARD_MS = 8000;           // driver card closes by itself after this

// Colours: deep navy, quiet greys, one papaya accent
const uint16_t BG = af1Rgb(4, 8, 18);
const uint16_t ROW = af1Rgb(12, 18, 32); // every other row
const uint16_t RULE = af1Rgb(40, 48, 66);
const uint16_t DIM = af1Rgb(120, 130, 150);
const uint16_t INK = af1Rgb(235, 238, 242);
const uint16_t PAPAYA = af1Rgb(255, 128, 0);

// List layout: header, then two columns of 11 rows
const int TOP = 44, ROW_H = 25, COL_W = 240, PER_COL = 11;

enum Mode { LIST, FEATURE, CARD };
Mode mode = LIST;
uint32_t since = 0; // millis() when the current mode started
int cardNum = 0;    // car number shown on the driver card

// "1:23.456", or "-" when there's no time yet
void lapTime(char *out, size_t n, int32_t ms)
{
  if (ms <= 0) snprintf(out, n, "-");
  else snprintf(out, n, "%d:%02d.%03d", (int)(ms / 60000), (int)(ms / 1000 % 60), (int)(ms % 1000));
}

// "LEADER", "+1 LAP", "+12.3", or "" when unknown
void gapText(char *out, size_t n, const F1Driver *d)
{
  if (d->pos == 1) snprintf(out, n, "LEADER");
  else if (d->lapsDown) snprintf(out, n, "+%d LAP%s", d->lapsDown, d->lapsDown > 1 ? "S" : "");
  else if (d->gapMs < 0) out[0] = 0;
  else snprintf(out, n, "+%d.%d", (int)(d->gapMs / 1000), (int)(d->gapMs / 100 % 10));
}

uint16_t tyreColour(char c)
{
  switch (c)
  {
  case 'S': return RGB565_RED;
  case 'M': return RGB565_YELLOW;
  case 'H': return RGB565_WHITE;
  case 'I': return af1Rgb(40, 200, 80);
  default: return af1Rgb(60, 140, 255); // 'W'
  }
}

// Tyre as a coloured ring with its letter, centred on (x, y); nothing if unknown
void tyre(int x, int y, char c)
{
  if (!c) return;
  uint16_t col = tyreColour(c);
  gfx->drawCircle(x, y, 8, col);
  gfx->drawCircle(x, y, 7, col);
  char s[2] = {c, 0};
  af1Text(x - 2, y - 3, 1, col, s);
}

void rightText(int right, int y, int size, uint16_t colour, const char *s)
{
  af1Text(right - (int)strlen(s) * 6 * size, y, size, colour, s);
}

void header()
{
  char s[64];
  if (race.lap)
  {
    int n = snprintf(s, sizeof(s), "LAP %d", race.lap);
    af1Text(14, 12, 2, PAPAYA, s);
    snprintf(s, sizeof(s), "/ %d", race.totalLaps);
    af1Text(14 + 12 * (n + 1), 12, 2, DIM, s);
  }
  else
    af1Text(14, 12, 2, PAPAYA, "GRID");

  snprintf(s, sizeof(s), "%s - %s", race.circuit, race.sessionName);
  af1Centred(s, 16, 1, DIM);

  struct tm t;
  if (getLocalTime(&t, 0))
  {
    snprintf(s, sizeof(s), "%d:%02d %s", t.tm_hour % 12 ? t.tm_hour % 12 : 12, t.tm_min, t.tm_hour < 12 ? "am" : "pm");
    rightText(466, 16, 1, DIM, s);
  }
  gfx->drawFastHLine(14, 36, 452, RULE);
}

void drawList()
{
  header();
  gfx->drawFastVLine(COL_W, TOP + 4, 320 - TOP - 8, RULE);
  for (int i = 0; i < 2 * PER_COL; i++)
  {
    int x = i / PER_COL * COL_W, y = TOP + i % PER_COL * ROW_H;
    if (i % 2) gfx->fillRect(x + 4, y, COL_W - 8, ROW_H, ROW);
    F1Driver *d = f1DriverAt(i + 1);
    if (!d) continue;
    char s[16];
    snprintf(s, sizeof(s), "%d", i + 1);
    rightText(x + 32, y + 5, 2, DIM, s);
    gfx->fillRect(x + 38, y + 4, 3, 17, d->colour);
    af1Text(x + 48, y + 5, 2, INK, d->code);
    gapText(s, sizeof(s), d);
    rightText(x + 176, y + 5, 2, d->pos == 1 ? PAPAYA : DIM, s);
    if (d->compound)
    {
      tyre(x + 194, y + 12, d->compound);
      snprintf(s, sizeof(s), "%d", d->tyreLaps);
      af1Text(x + 208, y + 5, 2, DIM, s);
    }
  }
}

// One stat on the driver page: small grey label over a bigger value, in a 2-column grid
void stat(int i, const char *label, const char *value)
{
  int x = 196 + i % 2 * 140, y = 130 + i / 2 * 62;
  af1Text(x, y, 1, DIM, label);
  af1Text(x, y + 14, 2, INK, value);
}

// Full-screen driver page: the feature view (top 3) and the tap card share it.
// The team-colour panel on the left is where the photo will go.
void drawDriver(F1Driver *d, bool card)
{
  char s[48];
  gfx->fillRect(0, 0, 170, 320, d->colour);
  snprintf(s, sizeof(s), "P%d", d->pos);
  af1Text((170 - (int)strlen(s) * 48) / 2, 128, 8, RGB565_WHITE, s);

  af1Text(196, 28, 6, card ? d->colour : INK, d->code);
  if (card)
  {
    if (d->gridPos) snprintf(s, sizeof(s), "Started P%d - now P%d (%+d)", d->gridPos, d->pos, d->gridPos - d->pos);
    else snprintf(s, sizeof(s), "Now P%d", d->pos);
    af1Text(196, 90, 1, PAPAYA, s);
  }
  else
    af1Text(196, 90, 1, PAPAYA, "TOP 3");

  gapText(s, sizeof(s), d);
  stat(0, "GAP", s[0] ? s : "-");
  if (d->compound) snprintf(s, sizeof(s), "   %d LAPS", d->tyreLaps); // spaces leave room for the ring
  else snprintf(s, sizeof(s), "-");
  stat(1, "TYRE", s);
  tyre(344, 151, d->compound);
  lapTime(s, sizeof(s), d->bestLapMs);
  stat(2, "BEST LAP", s);
  if (card)
  {
    lapTime(s, sizeof(s), d->lastLapMs);
    stat(3, "LAST LAP", s);
    snprintf(s, sizeof(s), "%d", d->pitStops);
    stat(4, "PIT STOPS", s);
  }
}

void setMode(Mode m)
{
  mode = m;
  since = millis();
}

void draw()
{
  gfx->fillScreen(BG);
  if (race.ready && mode == LIST && millis() - since >= FEATURE_EVERY_MS) setMode(FEATURE);
  uint32_t t = millis() - since;
  if (!race.ready)
  {
    af1Text(14, 12, 2, PAPAYA, "AF1");
    af1Centred("Loading race...", 140, 2, INK);
    if (race.status[0]) af1Centred(race.status, 176, 1, DIM);
  }
  else if (mode == FEATURE)
  {
    int p = 1 + t / FEATURE_EACH_MS;
    F1Driver *d = p <= 3 ? f1DriverAt(p) : nullptr;
    if (d) drawDriver(d, false);
    else { setMode(LIST); drawList(); } // done with P3 (or nobody there): back to the list
  }
  else if (mode == CARD)
  {
    F1Driver *d = f1Driver(cardNum);
    if (d && t < CARD_MS) drawDriver(d, true);
    else { setMode(LIST); drawList(); }
  }
  else
    drawList();
  gfx->flush();
}

void onTap(int x, int y)
{
  if (mode != LIST) { setMode(LIST); return; }
  if (y < TOP || y >= TOP + PER_COL * ROW_H) return;
  F1Driver *d = f1DriverAt(x / COL_W * PER_COL + (y - TOP) / ROW_H + 1);
  if (!d) return;
  cardNum = d->num;
  setMode(CARD);
}

void setup()
{
  Serial.begin(115200);
  af1Begin();
  af1NetBegin("af1-standings");
  f1Begin(SESSION_KEY, REPLAY_SPEED, F1_POSITIONS | F1_LAPS | F1_STINTS | F1_INTERVALS);
  setMode(LIST);
}

void loop()
{
  af1NetTick();
  f1Tick();
  TouchEvent e = af1Touch();
  if (e.kind == T_TAP && race.ready) onTap(e.x, e.y);
  draw();
  delay(10);
}
