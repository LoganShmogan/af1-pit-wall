/*
  AF1 - Drama board (right screen).
  Lap, track status, the fight for the lead, fastest lap and pit stop, weather and a news feed.
  Big moments (crash, red flag, safety car) take over the screen with a card for 8 s;
  a lead change or fastest lap gets a smaller banner for 4 s.

  Touch:
    tap a news item  = open its card again
    tap a card       = close it
    tap the speaker  = mute / unmute (top right)
    hold the top bar = toy button: a test crash
*/
#include <AF1.h>

const long SESSION_KEY = 0; // 0 = latest finished race
const int REPLAY_SPEED = 1; // keep the same on all three boards so they stay in sync

const uint16_t BG = af1Rgb(6, 10, 22); // deep navy
const uint16_t PANEL = af1Rgb(18, 26, 46);
const uint16_t LINE = af1Rgb(40, 50, 75);
const uint16_t DIM = af1Rgb(125, 135, 155);
const uint16_t PAPAYA = af1Rgb(0xFF, 0x80, 0x00);
const uint16_t PURPLE = af1Rgb(170, 80, 255);
const uint16_t AMBER = af1Rgb(255, 200, 0);
const uint16_t RAIN = af1Rgb(90, 170, 255);

// One entry per F1NewsKind, in the same order as AF1.h
const char *KIND_NAME[] = {"LIGHTS OUT", "CHEQUERED FLAG", "NEW LEADER", "FASTEST LAP", "PIT STOP", "CRASH",
                           "SAFETY CAR", "VIRTUAL SAFETY CAR", "SAFETY CAR IN", "RED FLAG", "GREEN FLAG", "PENALTY"};
const uint16_t KIND_COLOUR[] = {RGB565_WHITE, RGB565_WHITE, PAPAYA, PURPLE, RAIN, RGB565_RED,
                                AMBER, AMBER, AMBER, RGB565_RED, RGB565_GREEN, DIM};
const int KIND_RANK[] = {0, 0, 2, 1, 0, 5, 3, 3, 0, 4, 0, 0}; // card priority: 0 = none, 3+ = full screen

// One entry per F1Track
const char *TRACK_SHORT[] = {"", "GREEN", "VSC", "SC", "RED", "CHEQUERED"};
const uint16_t TRACK_COLOUR[] = {BG, RGB565_GREEN, AMBER, AMBER, RGB565_RED, RGB565_WHITE};

const int FEED_Y = 46, ROW_H = 45; // news feed rows (right panel)

uint32_t nextSeq = 0; // next news item to handle
F1News queue[8];      // cards waiting their turn
int queued = 0;
F1News card;          // the card on screen
bool cardOn = false;
uint32_t cardAt = 0;
bool muted = false;
uint32_t lastClipAt = -3000; // so the very first clip isn't skipped

// The MP3 to play for a news item (build guide section 9), nullptr = no sound.
const char *clipFor(const F1News &n)
{
  static char crash[32];
  switch (n.kind)
  {
  case NEWS_START: return "lights out away we go.mp3";
  case NEWS_FINISH: return "chequered flag.mp3";
  case NEWS_LEAD: return "taken the lead.mp3";
  case NEWS_FASTEST: return "fastest lap.mp3";
  case NEWS_PIT: return "diving into the pits.mp3";
  case NEWS_CRASH:
    if (n.turn < 1 || n.turn > 20) return "yellow flags.mp3"; // corner unknown
    snprintf(crash, sizeof(crash), "crash at turn %02d.mp3", n.turn);
    return crash;
  case NEWS_SC: return "safety car is out.mp3";
  case NEWS_VSC: return "yellow flags.mp3";
  case NEWS_SC_ENDING: return "safety car coming in.mp3";
  case NEWS_GREEN: return "green flag we are racing.mp3";
  default: return nullptr; // red flag and penalty have no clip yet
  }
}

bool bigCard(const F1News &n) { return KIND_RANK[n.kind] >= 3; }

void showCard(const F1News &n)
{
  card = n;
  cardOn = true;
  cardAt = millis();
}

// A new news item: maybe a sound, maybe a card.
void handle(const F1News &n)
{
  const char *clip = n.kind == NEWS_PIT ? nullptr : clipFor(n); // pit stops are too frequent to call
  if (clip && !muted && millis() - lastClipAt >= 3000)          // never talk over the last clip
  {
    Serial.printf("SOUND: %s\n", clip);
    lastClipAt = millis();
  }
  if (KIND_RANK[n.kind] && queued < 8) queue[queued++] = n;
}

void readNews()
{
  if (race.newsCount - nextSeq > F1_NEWS_KEEP) nextSeq = race.newsCount - F1_NEWS_KEEP;
  while (nextSeq < race.newsCount) handle(*f1News(nextSeq++));
}

// Ends the card when its time is up, then shows the most important one waiting.
void updateCards()
{
  if (cardOn && millis() - cardAt >= (bigCard(card) ? 8000u : 4000u)) cardOn = false;
  if (cardOn || !queued) return;
  int best = 0;
  for (int i = 1; i < queued; i++)
    if (KIND_RANK[queue[i].kind] > KIND_RANK[queue[best].kind]) best = i;
  showCard(queue[best]);
  queued--;
  for (int i = best; i < queued; i++) queue[i] = queue[i + 1];
}

// Prints s word-wrapped to `cols` characters a line, at most `lines` lines.
void wrapText(int x, int y, int size, uint16_t colour, const char *s, int cols, int lines)
{
  char buf[96];
  for (int i = 0; i < lines && *s; i++)
  {
    int n = strlen(s);
    if (n > cols)
    {
      n = cols;
      while (n > 0 && s[n] != ' ') n--;
      if (!n) n = cols; // one long word: just cut it
    }
    snprintf(buf, sizeof(buf), "%.*s", n, s);
    af1Text(x, y + i * size * 10, size, colour, buf);
    s += n;
    while (*s == ' ') s++;
  }
}

// Team-colour tick + driver code ("---" if unknown)
void driverCode(int x, int y, int size, F1Driver *d)
{
  gfx->fillRect(x, y, 4, size * 8, d ? d->colour : DIM);
  af1Text(x + 10, y, size, RGB565_WHITE, d ? d->code : "---");
}

void topBar()
{
  char s[32];
  if (race.lap) snprintf(s, sizeof(s), "LAP %d / %d", race.lap, race.totalLaps);
  else strlcpy(s, "GRID", sizeof(s));
  af1Text(12, 12, 2, RGB565_WHITE, s);

  if (race.track)
  {
    const char *t = TRACK_SHORT[race.track];
    int w = strlen(t) * 12 + 16;
    gfx->fillRoundRect(176, 8, w, 24, 12, TRACK_COLOUR[race.track]);
    af1Text(184, 12, 2, race.track == TRACK_RED ? RGB565_WHITE : RGB565_BLACK, t);
  }

  struct tm tm;
  if (getLocalTime(&tm, 0))
  {
    snprintf(s, sizeof(s), "%d:%02d %s", tm.tm_hour % 12 ? tm.tm_hour % 12 : 12, tm.tm_min, tm.tm_hour < 12 ? "am" : "pm");
    af1Text(424 - strlen(s) * 12, 12, 2, DIM, s);
  }

  // Speaker icon, crossed out when muted
  gfx->fillRect(444, 15, 6, 10, DIM);
  gfx->fillTriangle(448, 20, 458, 10, 458, 30, DIM);
  if (muted)
  {
    gfx->drawLine(462, 14, 472, 26, RGB565_RED);
    gfx->drawLine(462, 26, 472, 14, RGB565_RED);
  }
  else
  {
    gfx->drawFastVLine(463, 16, 8, DIM);
    gfx->drawFastVLine(468, 12, 16, DIM);
  }
  gfx->drawFastHLine(0, 39, 480, LINE);
}

void leftPanel()
{
  char s[40];
  af1Text(12, 48, 1, DIM, "FIGHT FOR THE LEAD");
  F1Driver *p1 = f1DriverAt(1), *p2 = f1DriverAt(2);
  driverCode(12, 62, 3, p1);
  af1Text(96, 66, 2, DIM, "vs");
  driverCode(132, 62, 3, p2);
  if (p2 && p2->intervalMs >= 0) snprintf(s, sizeof(s), "GAP +%d.%03d", (int)(p2->intervalMs / 1000), (int)(p2->intervalMs % 1000));
  else strlcpy(s, "GAP --", sizeof(s));
  af1Text(12, 96, 2, PAPAYA, s);
  gfx->drawFastHLine(12, 122, 212, LINE);

  af1Text(12, 130, 1, DIM, "FASTEST LAP");
  if (race.fastestLapMs)
  {
    int ms = race.fastestLapMs;
    driverCode(12, 144, 2, f1Driver(race.fastestLapDrv));
    snprintf(s, sizeof(s), "%d:%02d.%03d", ms / 60000, ms / 1000 % 60, ms % 1000);
    af1Text(80, 144, 2, PURPLE, s);
    snprintf(s, sizeof(s), "LAP %d", race.fastestLapNo);
    af1Text(22, 164, 1, DIM, s);
  }
  else af1Text(12, 144, 2, DIM, "--");
  gfx->drawFastHLine(12, 180, 212, LINE);

  af1Text(12, 188, 1, DIM, "FASTEST STOP");
  if (race.fastestPitMs)
  {
    driverCode(12, 202, 2, f1Driver(race.fastestPitDrv));
    snprintf(s, sizeof(s), "%d.%d s", (int)(race.fastestPitMs / 1000), (int)(race.fastestPitMs % 1000 / 100));
    af1Text(80, 202, 2, RGB565_WHITE, s);
  }
  else af1Text(12, 202, 2, DIM, "--");
  gfx->drawFastHLine(12, 228, 212, LINE);

  af1Text(12, 236, 1, DIM, "WEATHER");
  if (race.hasWeather)
  {
    snprintf(s, sizeof(s), "AIR %.0fC", race.airC);
    af1Text(12, 250, 2, RGB565_WHITE, s);
    snprintf(s, sizeof(s), "TRACK %.0fC", race.trackC);
    af1Text(12, 272, 2, RGB565_WHITE, s);
    snprintf(s, sizeof(s), "HUMIDITY %d%%", race.humidity);
    af1Text(12, 294, 1, DIM, s);
    if (race.raining) af1Text(160, 250, 2, RAIN, "RAIN");
  }
  else af1Text(12, 250, 2, DIM, "--");
}

void newsFeed()
{
  char s[8];
  for (int i = 0; i < 6; i++)
  {
    const F1News *n = f1News(race.newsCount - 1 - i); // newest first; nullptr past the oldest
    if (!n) break;
    int y = FEED_Y + i * ROW_H;
    gfx->fillRect(248, y, 3, 30, KIND_COLOUR[n->kind]);
    snprintf(s, sizeof(s), "L%d", n->lap);
    af1Text(258, y, 1, DIM, s);
    af1Text(282, y, 1, KIND_COLOUR[n->kind], KIND_NAME[n->kind]);
    wrapText(258, y + 12, 2, RGB565_WHITE, n->text, 18, 1);
  }
  if (!race.newsCount) af1Text(258, FEED_Y, 1, DIM, "All quiet so far.");
}

void dashboard()
{
  gfx->fillScreen(BG);
  topBar();
  leftPanel();
  gfx->drawFastVLine(236, 48, 260, LINE);
  newsFeed();
}

// Full-screen card for big moments, a band across the middle for the rest.
void drawCard(const F1News &n)
{
  bool big = bigCard(n);
  int y = big ? 0 : 92;
  char s[32];
  if (big) gfx->fillScreen(BG);
  else gfx->fillRect(0, y, 480, 136, PANEL);
  gfx->fillRect(0, y, 480, 6, KIND_COLOUR[n.kind]);

  if (n.kind == NEWS_CRASH && n.turn) snprintf(s, sizeof(s), "CRASH - TURN %d", n.turn);
  else strlcpy(s, KIND_NAME[n.kind], sizeof(s));
  af1Text(20, y + 22, big ? 4 : 3, RGB565_WHITE, s);
  snprintf(s, sizeof(s), "LAP %d", n.lap);
  if (big) af1Text(20, 62, 2, DIM, s);
  else af1Text(460 - strlen(s) * 12, y + 26, 2, DIM, s);

  // Driver code(s) on team-colour panels
  int px = 20, py = y + (big ? 90 : 52), size = big ? 6 : 3;
  for (uint8_t num : {n.drv, n.drv2})
  {
    F1Driver *d = f1Driver(num);
    if (!d) continue;
    gfx->fillRect(px, py, size * 26, size * 15, d->colour);
    af1Text(px + size * 2 + 2, py + size * 4 + 2, size, RGB565_BLACK, d->code); // shadow
    af1Text(px + size * 2, py + size * 4, size, RGB565_WHITE, d->code);
    px += size * 26 + 16;
  }
  wrapText(20, y + (big ? 196 : 108), 2, RGB565_WHITE, n.text, 36, big ? 3 : 1);
  if (big) af1Centred("TAP TO CLOSE", 300, 1, DIM);
}

void draw()
{
  if (!race.ready)
  {
    gfx->fillScreen(BG);
    af1Centred("Loading race...", 140, 2, DIM);
    if (race.status[0]) af1Centred(race.status, 172, 1, PAPAYA);
  }
  else
  {
    if (!cardOn || !bigCard(card)) dashboard();
    if (cardOn) drawCard(card);
  }
  gfx->flush();
}

void touch(const TouchEvent &e)
{
  if (e.kind == T_HOLD && e.y < 40) // toy button: a test crash at turn 4 for the leader
  {
    F1News n = {};
    F1Driver *d = f1DriverAt(1);
    n.t = race.clockMs;
    n.kind = NEWS_CRASH;
    n.drv = d ? d->num : 0;
    n.lap = race.lap;
    n.turn = 4;
    strlcpy(n.text, "Toy button: a test crash", sizeof(n.text));
    f1PushNews(n);
  }
  if (e.kind != T_TAP) return;
  if (cardOn) cardOn = false;
  else if (e.x >= 430 && e.y < 40) muted = !muted;
  else if (e.x >= 240 && e.y >= FEED_Y)
  {
    const F1News *n = f1News(race.newsCount - 1 - (e.y - FEED_Y) / ROW_H);
    if (n) showCard(*n);
  }
}

void setup()
{
  Serial.begin(115200);
  af1Begin();
  af1NetBegin("af1-drama");
  f1Begin(SESSION_KEY, REPLAY_SPEED, F1_POSITIONS | F1_LAPS | F1_INTERVALS | F1_PITS | F1_RACE_CONTROL | F1_WEATHER);
}

void loop()
{
  af1NetTick();
  f1Tick();
  TouchEvent e = af1Touch();
  if (race.ready) touch(e);
  readNews();
  updateCards();

  static uint32_t lastDraw = 0;
  if (millis() - lastDraw >= 100)
  {
    lastDraw = millis();
    draw();
  }
  delay(10);
}
