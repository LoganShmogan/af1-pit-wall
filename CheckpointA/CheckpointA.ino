/*
  AF1 - Checkpoint A: NZ clock + a replayed race's lap count, on any board.
  Uses the shared AF1 library (Jobs 3-5): screen, touch, wifi/clock, OpenF1 replay.

  CHANGE BEFORE UPLOADING: BOARD_NAME below ("STANDINGS", "MAP" or "DRAMA").

  Touch:
    tap  = replay speed 1x -> 10x -> 60x
    hold = restart the replay
*/
#include <AF1.h>

const char *BOARD_NAME = "MAP";
const long SESSION_KEY = 0; // 0 = latest finished race, or an OpenF1 session_key (e.g. 11404 = Mexico 2026)

const int SPEEDS[] = {1, 10, 60};
int speedIdx = 0;

const char *DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char *MONTHS[] = {"January", "February", "March", "April", "May", "June", "July",
                        "August", "September", "October", "November", "December"};

void draw()
{
  char line[96];
  gfx->fillScreen(RGB565_BLACK);
  af1Text(10, 8, 2, RGB565_DARKGREY, BOARD_NAME);
  af1Text(400, 8, 2, af1WifiOk() ? RGB565_GREEN : RGB565_RED, af1WifiOk() ? "WIFI" : "NO WIFI");

  struct tm t;
  if (af1TimeOk() && getLocalTime(&t, 0))
  {
    int h12 = t.tm_hour % 12 ? t.tm_hour % 12 : 12;
    snprintf(line, sizeof(line), "%d:%02d:%02d %s", h12, t.tm_min, t.tm_sec, t.tm_hour < 12 ? "am" : "pm");
    af1Centred(line, 40, 5, RGB565_WHITE);
    snprintf(line, sizeof(line), "%s %d %s", DAYS[t.tm_wday], t.tm_mday, MONTHS[t.tm_mon]);
    af1Centred(line, 90, 2, RGB565_LIGHTGREY);
  }
  else
    af1Centred(af1WifiOk() ? "Getting the time..." : "Joining wifi...", 60, 3, RGB565_YELLOW);

  gfx->drawFastHLine(20, 125, 440, RGB565_DARKGREY);

  if (race.ready)
  {
    snprintf(line, sizeof(line), "%s - %s (replay x%d)", race.circuit, race.sessionName, SPEEDS[speedIdx]);
    af1Centred(line, 140, 2, RGB565_LIGHTGREY);
    if (race.lap)
      snprintf(line, sizeof(line), "Lap %d / %d", race.lap, race.totalLaps);
    else
      strlcpy(line, "On the grid", sizeof(line));
    af1Centred(line, 170, 4, af1Rgb(0xFF, 0x80, 0x00)); // papaya

    int64_t s = (race.clockMs - race.startMs) / 1000;
    snprintf(line, sizeof(line), "Race clock %d:%02d:%02d", (int)(s / 3600), (int)(s / 60 % 60), (int)(s % 60));
    af1Centred(line, 215, 2, RGB565_DARKGREY);

    for (int p = 1; p <= 3; p++)
    {
      F1Driver *d = f1DriverAt(p);
      if (!d) continue;
      int x = 40 + (p - 1) * 145;
      gfx->fillRect(x, 250, 6, 40, d->colour);
      snprintf(line, sizeof(line), "P%d %s", p, d->code);
      af1Text(x + 14, 260, 3, RGB565_WHITE, line);
    }
  }
  else
    af1Centred("Loading race data...", 170, 2, RGB565_YELLOW);

  if (race.status[0]) af1Centred(race.status, 300, 1, RGB565_RED);
  gfx->flush();
}

void setup()
{
  Serial.begin(115200);
  af1Begin();
  char host[24];
  snprintf(host, sizeof(host), "af1-%s", BOARD_NAME);
  af1NetBegin(host);
  f1Begin(SESSION_KEY, SPEEDS[speedIdx]);
}

void loop()
{
  af1NetTick();
  f1Tick();

  TouchEvent e = af1Touch();
  if (e.kind == T_TAP)
  {
    speedIdx = (speedIdx + 1) % 3;
    f1SetSpeed(SPEEDS[speedIdx]);
  }
  else if (e.kind == T_HOLD)
    f1Restart();

  static uint32_t lastDraw = 0;
  if (millis() - lastDraw >= 250)
  {
    lastDraw = millis();
    draw();
  }
  delay(10);
}
