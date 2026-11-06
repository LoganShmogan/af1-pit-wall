// Race data from OpenF1, replayed as if live (free historical data, Option C).
// A whole race is small (about 560 position changes, 72 lap start times), so
// everything is loaded once, then played back against a race clock.
#include "AF1.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ponytail: live data (Job 24) swaps this URL and adds the auth token header in get()
#define API "https://api.openf1.org/v1/"
#define REQ_GAP_MS 600 // free tier: 3 requests/second, 30/minute
#define EV_MAX 4000
#define LAP_MAX 100

F1Race race;

struct PosEvent { int64_t t; uint8_t drv, pos; };
static PosEvent *ev = nullptr;
static int evCount = 0, evNext = 0;
static int64_t lapStart[LAP_MAX + 1]; // earliest start of each lap = when the leader began it
static long wantKey = 0;
static int speed = 1;
static enum { LOAD_SESSION, LOAD_DRIVERS, LOAD_LAPS, LOAD_POSITIONS, PLAYING } stage = LOAD_SESSION;
static uint32_t nextReqAt = 0;
static int64_t baseRace = 0, lastClock = 0;
static uint32_t baseWall = 0;

// "2026-11-01T20:03:25.510000+00:00" -> UTC ms. OpenF1 always sends +00:00.
int64_t af1ParseIso(const char *s)
{
  if (!s || strlen(s) < 19) return 0;
  int Y = atoi(s), M = atoi(s + 5), D = atoi(s + 8);
  int h = atoi(s + 11), mi = atoi(s + 14), se = atoi(s + 17);
  int ms = 0;
  if (s[19] == '.')
    for (int i = 0, mul = 100; i < 3 && isdigit((unsigned char)s[20 + i]); i++, mul /= 10)
      ms += (s[20 + i] - '0') * mul;
  // days since 1970 (Howard Hinnant's days_from_civil)
  int y = Y - (M <= 2);
  int era = y / 400;
  int yoe = y - era * 400;
  int doy = (153 * ((M + 9) % 12) + 2) / 5 + D - 1;
  int64_t days = (int64_t)era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
  return (((days * 24 + h) * 60 + mi) * 60 + se) * 1000 + ms;
}

// ---------- HTTP: stream a JSON array one element at a time ----------
typedef void (*ElemCb)(JsonDocument &d);

// Returns HTTP code (200 ok, 404 = OpenF1's "no results"), or <0 on network error.
static int get(const char *url, JsonDocument &filter, ElemCb cb)
{
  static WiFiClientSecure tls;
  tls.setInsecure(); // ponytail: public read-only data; pin the CA cert if this ever sends a token
  HTTPClient http;
  http.useHTTP10(true); // no chunked replies, so ArduinoJson can read the stream directly
  http.setTimeout(15000);
  if (!http.begin(tls, url)) return -1;
  int code = http.GET();
  Serial.printf("GET %s -> %d\n", url, code);
  if (code == 200)
  {
    Stream &s = http.getStream();
    if (s.find("["))
      do
      {
        JsonDocument d;
        if (deserializeJson(d, s, DeserializationOption::Filter(filter))) break;
        cb(d);
      } while (s.findUntil(",", "]"));
  }
  http.end();
  return code;
}

// Records why a request failed and when to try again. Returns true if it did fail.
static bool failed(int code, const char *what)
{
  if (code == 200) return false;
  uint32_t wait = 10000;
  if (code == 401 || code == 403)
  {
    snprintf(race.status, sizeof(race.status), "OpenF1 refused %s (%d) - locked during live sessions?", what, code);
    wait = 60000;
  }
  else if (code == 429)
  {
    snprintf(race.status, sizeof(race.status), "Too many requests - waiting 30 s");
    wait = 30000;
  }
  else if (code == 404)
    snprintf(race.status, sizeof(race.status), "No %s data for this session yet", what);
  else if (code < 0)
    snprintf(race.status, sizeof(race.status), "Can't reach OpenF1 (%s)", HTTPClient::errorToString(code).c_str());
  else
    snprintf(race.status, sizeof(race.status), "OpenF1 error %d loading %s", code, what);
  Serial.println(race.status);
  nextReqAt = millis() + wait;
  return true;
}

// ---------- Callbacks ----------
static void onSession(JsonDocument &d)
{
  int64_t end = af1ParseIso(d["date_end"] | "");
  if (race.sessionKey && end <= race.endMs) return; // keep the most recent one
  race.sessionKey = d["session_key"] | 0L;
  race.startMs = af1ParseIso(d["date_start"] | "");
  race.endMs = end;
  strlcpy(race.sessionName, d["session_name"] | "", sizeof(race.sessionName));
  strlcpy(race.circuit, d["circuit_short_name"] | "", sizeof(race.circuit));
  strlcpy(race.country, d["country_name"] | "", sizeof(race.country));
}

static void onDriver(JsonDocument &d)
{
  int num = d["driver_number"] | 0;
  if (num <= 0 || num > 99 || race.driverCount >= 24) return;
  for (int i = 0; i < race.driverCount; i++)
    if (race.drivers[i].num == num) return;
  F1Driver &dr = race.drivers[race.driverCount++];
  dr.num = num;
  strlcpy(dr.code, d["name_acronym"] | "???", sizeof(dr.code));
  long c = strtol(d["team_colour"] | "FFFFFF", nullptr, 16);
  dr.colour = af1Rgb(c >> 16, c >> 8, c);
  dr.pos = 0;
}

static void onLap(JsonDocument &d)
{
  int n = d["lap_number"] | 0;
  int64_t t = af1ParseIso(d["date_start"] | "");
  if (n < 1 || n > LAP_MAX || !t) return;
  if (!lapStart[n] || t < lapStart[n]) lapStart[n] = t;
  if (n > race.totalLaps) race.totalLaps = n;
}

static void onPosition(JsonDocument &d)
{
  int drv = d["driver_number"] | 0, pos = d["position"] | 0;
  int64_t t = af1ParseIso(d["date"] | "");
  if (evCount >= EV_MAX || drv <= 0 || drv > 99 || pos <= 0 || pos > 30 || !t) return;
  ev[evCount++] = {t, (uint8_t)drv, (uint8_t)pos};
}

static int byTime(const void *a, const void *b)
{
  int64_t d = ((const PosEvent *)a)->t - ((const PosEvent *)b)->t;
  return d < 0 ? -1 : d > 0;
}

// ---------- Loading ----------
static void loadStep()
{
  char url[160];
  JsonDocument f;
  int code;
  switch (stage)
  {
  case LOAD_SESSION:
    race.sessionKey = 0;
    if (wantKey)
      snprintf(url, sizeof(url), API "sessions?session_key=%ld", wantKey);
    else
    {
      // Latest finished race. ponytail: in January, before round 1, this finds nothing; pass a session key.
      time_t now = time(nullptr);
      struct tm t;
      gmtime_r(&now, &t);
      char iso[24];
      strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%S", &t);
      snprintf(url, sizeof(url), API "sessions?year=%d&session_name=Race&date_end<%s", t.tm_year + 1900, iso);
    }
    f["session_key"] = f["date_start"] = f["date_end"] = f["session_name"] = true;
    f["circuit_short_name"] = f["country_name"] = true;
    code = get(url, f, onSession);
    if (failed(code, "sessions")) return;
    if (!race.sessionKey) { failed(404, "session"); return; }
    stage = LOAD_DRIVERS;
    break;
  case LOAD_DRIVERS:
    snprintf(url, sizeof(url), API "drivers?session_key=%ld", race.sessionKey);
    f["driver_number"] = f["name_acronym"] = f["team_colour"] = true;
    race.driverCount = 0;
    if (failed(get(url, f, onDriver), "drivers")) return;
    stage = LOAD_LAPS;
    break;
  case LOAD_LAPS:
    snprintf(url, sizeof(url), API "laps?session_key=%ld", race.sessionKey);
    f["lap_number"] = f["date_start"] = true;
    memset(lapStart, 0, sizeof(lapStart));
    race.totalLaps = 0;
    if (failed(get(url, f, onLap), "laps")) return;
    stage = LOAD_POSITIONS;
    break;
  default: // LOAD_POSITIONS
    snprintf(url, sizeof(url), API "position?session_key=%ld", race.sessionKey);
    f["date"] = f["driver_number"] = f["position"] = true;
    evCount = 0;
    if (failed(get(url, f, onPosition), "positions")) return;
    qsort(ev, evCount, sizeof(PosEvent), byTime);
    if (evCount && ev[evCount - 1].t > race.endMs) race.endMs = ev[evCount - 1].t; // races can overrun
    Serial.printf("Loaded %s %s: %d drivers, %d laps, %d position changes\n",
                  race.circuit, race.sessionName, race.driverCount, race.totalLaps, evCount);
    stage = PLAYING;
    race.ready = true;
    race.status[0] = 0;
    f1Restart();
    break;
  }
  nextReqAt = millis() + REQ_GAP_MS;
}

// ---------- Playback ----------
static void play()
{
  int64_t clock = baseRace + (int64_t)(millis() - baseWall) * speed;
  if (clock > race.endMs) clock = race.endMs;
  if (clock < lastClock) // went backwards (restart): replay from the top
  {
    evNext = 0;
    for (int i = 0; i < race.driverCount; i++) race.drivers[i].pos = 0;
  }
  lastClock = race.clockMs = clock;

  for (; evNext < evCount && ev[evNext].t <= clock; evNext++)
    for (int i = 0; i < race.driverCount; i++)
      if (race.drivers[i].num == ev[evNext].drv) race.drivers[i].pos = ev[evNext].pos;

  race.lap = 0;
  for (int n = 1; n <= race.totalLaps; n++)
    if (lapStart[n] && lapStart[n] <= clock) race.lap = n;
}

void f1Begin(long sessionKey, int s)
{
  wantKey = sessionKey;
  speed = s;
  if (!ev) ev = (PosEvent *)ps_malloc(EV_MAX * sizeof(PosEvent));
  race = F1Race();
  stage = LOAD_SESSION;
  nextReqAt = 0;
}

void f1Tick()
{
  if (stage == PLAYING) { play(); return; }
  if (millis() >= nextReqAt && af1WifiOk() && af1TimeOk()) loadStep();
}

void f1SetSpeed(int s)
{
  baseRace = race.clockMs;
  baseWall = millis();
  speed = s;
}

void f1Restart()
{
  baseRace = race.startMs;
  baseWall = millis();
  lastClock = INT64_MAX; // forces a reset on the next play()
}

F1Driver *f1DriverAt(int pos)
{
  for (int i = 0; i < race.driverCount; i++)
    if (race.drivers[i].pos == pos) return &race.drivers[i];
  return nullptr;
}
