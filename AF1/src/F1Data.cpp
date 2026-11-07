// Race data from OpenF1, replayed as if live (free historical data, Option C).
//
// Small feeds (laps, positions, tyres, pits, race control, weather) are loaded once and played
// back against a race clock. Big feeds (intervals, car locations) are streamed in windows just
// ahead of the clock. On the ESP32 the downloads run in a background task on the other core, so
// drawing never stalls; on the PC (tests) they run inside f1Tick().
//
// The race clock comes from the NTP time, so every board shows the same moment of the replay
// without talking to the others, and the replay loops forever.
#include "AF1.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <miniz.h> // the ESP32 ROM decompressor (tinfl)

#ifdef ARDUINO
#define LOCK() xSemaphoreTake(lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(lock)
static SemaphoreHandle_t lock;
#else
#define LOCK()
#define UNLOCK()
#endif

// ponytail: live data (Job 24) swaps this URL and adds the auth token header in get()
#define API "https://api.openf1.org/v1/"
#define LOAD_GAP_MS 600    // between one-off loads (free tier: 3 requests/second)
#define STREAM_GAP_MS 2000 // between streamed windows (free tier: 30/minute for the whole house)
#define EV_MAX 12000
#define RC_MAX 400
#define WX_MAX 400
#define STINT_MAX 200
#define AFTER_END_MS (30 * 60000) // replay span = scheduled session + 30 min (races overrun)
#define NEWS_FRESH_MS 30000       // events older than this when first seen make no news

F1Race race;

// ---------- Stored race ----------
enum { EV_POS, EV_LAP, EV_LAPTIME, EV_PIT, EV_RC, EV_WX, EV_GAP, EV_LOC };
struct Ev
{
  int64_t t;
  int32_t v;  // POS: position, LAPTIME/PIT: ms, RC/WX: index, GAP: gap ms (or -laps), LOC: x
  uint16_t w; // LAP/LAPTIME/PIT: lap, GAP: interval in 10 ms (0xFFFF unknown), LOC: y
  uint8_t type, drv;
};
struct Rc { char msg[96]; char category[12]; char flag[14]; uint8_t drv; };
struct Wx { float air, track; uint8_t humidity, rain; };
struct Stint { uint8_t drv, lapStart; char compound; };

static Ev *ev = nullptr;
static int evCount = 0, evNext = 0;
static Rc *rc = nullptr;
static int rcCount = 0;
static Wx wx[WX_MAX];
static int wxCount = 0;
static Stint stints[STINT_MAX];
static int stintCount = 0;

// Streamed feeds: a ring buffer filled by the loader, emptied by play()
typedef void (*ElemCb)(JsonDocument &d);
struct Feed
{
  uint16_t feed;
  const char *endpoint;
  const char *fields[4];
  ElemCb cb;
  int64_t windowMs;
  int cap;
  Ev *buf;
  int head, count;
  int64_t fetchedUntil;
};
static void onInterval(JsonDocument &d);
static void onLocation(JsonDocument &d);
static Feed streams[] = {
    {F1_INTERVALS, "intervals", {"date", "driver_number", "gap_to_leader", "interval"}, onInterval, 60000, 4096},
    {F1_LOCATION, "location", {"date", "driver_number", "x", "y"}, onLocation, 15000, 16384},
};
static const int STREAMS = sizeof(streams) / sizeof(streams[0]);
#define STAGING_MAX 16384 // rows of one window being downloaded (the biggest feed's buffer)
static Ev *staging = nullptr;
static int stagingCount = 0;

// Smoothing for car locations: the last few samples per car number
struct LocHist { int64_t t[4]; int16_t x[4], y[4]; uint8_t n; };
static LocHist hist[100];
#define LOC_DELAY_MS 600 // draw cars this far behind the clock so there's a sample either side

// ---------- Loader state ----------
static long wantKey = 0;
static uint16_t feeds = 0;
static int speed = 1;
static int64_t offset = 0; // clock = start + ((now * speed + offset) mod span)
static enum { LOAD_SESSION, LOAD_DRIVERS, LOAD_FEEDS, PLAYING } stage = LOAD_SESSION;
static int loadIdx = 0;
static uint32_t nextReqAt = 0;
static int64_t lastClock = 0;

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

static void isoOf(int64_t ms, char *out, size_t n)
{
  time_t s = (time_t)(ms / 1000);
  struct tm t;
  gmtime_r(&s, &t);
  snprintf(out, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
           t.tm_hour, t.tm_min, t.tm_sec, (int)(ms % 1000));
}

// ---------- HTTP ----------
// Replies are fetched gzip-compressed (15-30x smaller): the board's link to OpenF1 is slow
// (about 350 ms round trip from NZ, and the ESP32 only accepts ~5.7 KB per round trip).
// The whole reply goes into PSRAM, is unzipped with the ESP32's ROM decompressor, then parsed.

// Collects a reply in PSRAM
struct Sink : Stream
{
  char *buf = nullptr;
  size_t len = 0, cap = 0;
  size_t write(uint8_t c) { return write(&c, 1); }
  size_t write(const uint8_t *p, size_t n)
  {
    if (len + n > cap)
    {
      cap = (len + n) * 2;
      buf = (char *)ps_realloc(buf, cap);
    }
    memcpy(buf + len, p, n);
    len += n;
    return n;
  }
  int available() { return 0; }
  int read() { return -1; }
  int peek() { return -1; }
};

// Gzip -> plain text, in place. Returns false if it's damaged. Plain replies are left alone.
static bool gunzip(char *&buf, size_t &len)
{
  const uint8_t *b = (const uint8_t *)buf;
  if (len < 2 || b[0] != 0x1f || b[1] != 0x8b) return true;
  if (len < 18 || b[3] != 0) return false; // ponytail: OpenF1 (nginx) never sends the optional gzip fields
  size_t size = b[len - 4] | b[len - 3] << 8 | b[len - 2] << 16 | (size_t)b[len - 1] << 24;
  if (size > 4000000) return false; // no reply is this big: damaged
  char *out = (char *)ps_malloc(size + 1);
  size_t got = tinfl_decompress_mem_to_mem(out, size, b + 10, len - 18, 0);
  free(buf);
  buf = out;
  len = got;
  return got == size;
}

// Reads a JSON array in memory one element at a time (keeps ArduinoJson's memory use tiny)
struct MemReader
{
  const char *p, *end;
  int read() { return p < end ? (uint8_t)*p++ : -1; }
  size_t readBytes(char *out, size_t n)
  {
    size_t k = 0;
    for (; k < n && p < end; k++) out[k] = *p++;
    return k;
  }
};

static void parse(const char *json, size_t len, JsonDocument &filter, ElemCb cb)
{
  MemReader r{json, json + len};
  int c;
  while ((c = r.read()) >= 0 && c != '[') {} // up to the start of the array
  while (c >= 0)
  {
    JsonDocument d;
    if (deserializeJson(d, r, DeserializationOption::Filter(filter))) break;
    cb(d);
    while ((c = r.read()) >= 0 && c != ',' && c != ']') {} // to the next element, or the end
    if (c != ',') break;
  }
}

// Returns HTTP code (200 ok, 404 = OpenF1's "no results"), or <0 on network error.
static int get(const char *url, JsonDocument &filter, ElemCb cb)
{
  static WiFiClientSecure tls;
  tls.setInsecure(); // ponytail: public read-only data; pin the CA cert if this ever sends a token
  HTTPClient http;
  http.setAcceptEncoding("gzip");
  http.setTimeout(15000);
  if (!http.begin(tls, url)) return -1;
  int code = http.GET();
  Serial.printf("GET %s -> %d\n", url, code);
  if (code == 200)
  {
    Sink reply;
    int n = http.writeToStream(&reply); // handles chunked replies and waits for the network
    if (n < 0) code = n;
    else if (!gunzip(reply.buf, reply.len)) code = HTTPC_ERROR_READ_TIMEOUT; // cut off part-way
    else parse(reply.buf, reply.len, filter, cb);
    free(reply.buf);
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

// ---------- Parsing ----------
static void addEv(int64_t t, uint8_t type, int drv, int32_t v, uint16_t w)
{
  if (!t || evCount >= EV_MAX) return;
  ev[evCount++] = {t, v, w, type, (uint8_t)drv};
}

static bool goodDriver(int n) { return n > 0 && n < 100; }

static void onSession(JsonDocument &d)
{
  int64_t end = af1ParseIso(d["date_end"] | "");
  if (race.sessionKey && end <= race.endMs) return; // keep the most recent one
  race.sessionKey = d["session_key"] | 0L;
  race.circuitKey = d["circuit_key"] | 0;
  race.startMs = af1ParseIso(d["date_start"] | "");
  race.endMs = end;
  strlcpy(race.sessionName, d["session_name"] | "", sizeof(race.sessionName));
  strlcpy(race.circuit, d["circuit_short_name"] | "", sizeof(race.circuit));
  strlcpy(race.country, d["country_name"] | "", sizeof(race.country));
}

static void onDriver(JsonDocument &d)
{
  int num = d["driver_number"] | 0;
  if (!goodDriver(num) || race.driverCount >= 24 || f1Driver(num)) return;
  F1Driver &dr = race.drivers[race.driverCount++];
  dr = F1Driver();
  dr.num = num;
  strlcpy(dr.code, d["name_acronym"] | "???", sizeof(dr.code));
  long c = strtol(d["team_colour"] | "FFFFFF", nullptr, 16);
  dr.colour = af1Rgb(c >> 16, c >> 8, c);
}

static void onLap(JsonDocument &d)
{
  int n = d["lap_number"] | 0, drv = d["driver_number"] | 0;
  int64_t t = af1ParseIso(d["date_start"] | "");
  float secs = d["lap_duration"] | 0.0f;
  if (n < 1 || n > 255 || !goodDriver(drv)) return;
  addEv(t, EV_LAP, drv, 0, n);
  if (t && secs > 0) addEv(t + (int64_t)(secs * 1000), EV_LAPTIME, drv, (int32_t)(secs * 1000 + 0.5f), n);
  if (t && n > race.totalLaps) race.totalLaps = n;
}

static void onPosition(JsonDocument &d)
{
  int drv = d["driver_number"] | 0, pos = d["position"] | 0;
  if (goodDriver(drv) && pos > 0 && pos <= 30) addEv(af1ParseIso(d["date"] | ""), EV_POS, drv, pos, 0);
}

static void onStint(JsonDocument &d)
{
  int drv = d["driver_number"] | 0, lap = d["lap_start"] | 0;
  const char *c = d["compound"] | "";
  if (!goodDriver(drv) || lap < 1 || !c[0] || stintCount >= STINT_MAX) return;
  stints[stintCount++] = {(uint8_t)drv, (uint8_t)lap, c[0]}; // "SOFT" -> 'S', "INTERMEDIATE" -> 'I'
}

static void onPit(JsonDocument &d)
{
  int drv = d["driver_number"] | 0;
  float stop = d["stop_duration"] | 0.0f, lane = d["pit_duration"] | 0.0f;
  float secs = stop > 0 ? stop : lane;
  if (goodDriver(drv) && secs > 0) // rows without times are cars leaving for the grid
    addEv(af1ParseIso(d["date"] | ""), EV_PIT, drv, (int32_t)(secs * 1000 + 0.5f), d["lap_number"] | 0);
}

static void onRaceControl(JsonDocument &d)
{
  if (rcCount >= RC_MAX) return;
  Rc &r = rc[rcCount];
  strlcpy(r.msg, d["message"] | "", sizeof(r.msg));
  strlcpy(r.category, d["category"] | "", sizeof(r.category));
  strlcpy(r.flag, d["flag"] | "", sizeof(r.flag));
  r.drv = d["driver_number"] | 0;
  addEv(af1ParseIso(d["date"] | ""), EV_RC, 0, rcCount++, 0);
}

static void onWeather(JsonDocument &d)
{
  if (wxCount >= WX_MAX) return;
  wx[wxCount] = {d["air_temperature"] | 0.0f, d["track_temperature"] | 0.0f,
                 (uint8_t)(d["humidity"] | 0.0f), (uint8_t)((d["rainfall"] | 0.0f) > 0)};
  addEv(af1ParseIso(d["date"] | ""), EV_WX, 0, wxCount++, 0);
}

static void stage1(int64_t t, uint8_t type, int drv, int32_t v, uint16_t w)
{
  if (t && goodDriver(drv) && stagingCount < STAGING_MAX) staging[stagingCount++] = {t, v, w, type, (uint8_t)drv};
}

// "+1 LAP" / "+2 LAPS" -> -1 / -2; numbers -> ms; null -> -1000000 (unknown)
static int32_t gapOf(JsonVariant g)
{
  if (g.is<float>()) return (int32_t)(g.as<float>() * 1000 + 0.5f);
  const char *s = g | "";
  return s[0] == '+' ? -atoi(s + 1) : -1000000;
}

static void onInterval(JsonDocument &d)
{
  int32_t gap = gapOf(d["gap_to_leader"]), iv = gapOf(d["interval"]);
  uint16_t w = iv >= 0 && iv < 655340 ? iv / 10 : 0xFFFF;
  stage1(af1ParseIso(d["date"] | ""), EV_GAP, d["driver_number"] | 0, gap, w);
}

static void onLocation(JsonDocument &d)
{
  int x = d["x"] | 0, y = d["y"] | 0;
  if (x || y) stage1(af1ParseIso(d["date"] | ""), EV_LOC, d["driver_number"] | 0, x, (uint16_t)(int16_t)y);
}

static int byTime(const void *a, const void *b)
{
  int64_t d = ((const Ev *)a)->t - ((const Ev *)b)->t;
  return d < 0 ? -1 : d > 0;
}

// ---------- Loading ----------
struct Load { uint16_t feed; const char *endpoint; const char *fields[5]; ElemCb cb; const char *what; };
static const Load LOADS[] = {
    {F1_LAPS, "laps", {"lap_number", "driver_number", "date_start", "lap_duration"}, onLap, "laps"},
    {F1_POSITIONS, "position", {"date", "driver_number", "position"}, onPosition, "positions"},
    {F1_STINTS, "stints", {"driver_number", "lap_start", "compound"}, onStint, "tyres"},
    {F1_PITS, "pit", {"date", "driver_number", "lap_number", "pit_duration", "stop_duration"}, onPit, "pit stops"},
    {F1_RACE_CONTROL, "race_control", {"date", "driver_number", "category", "flag", "message"}, onRaceControl, "race control"},
    {F1_WEATHER, "weather", {"date", "air_temperature", "track_temperature", "humidity", "rainfall"}, onWeather, "weather"},
};
static const int LOAD_COUNT = sizeof(LOADS) / sizeof(LOADS[0]);

static void startPlaying()
{
  qsort(ev, evCount, sizeof(Ev), byTime);
  if (race.endMs < race.startMs) race.endMs = race.startMs; // no end date in the data
  race.endMs += AFTER_END_MS;
  Serial.printf("Loaded %s %s: %d drivers, %d laps, %d events\n", race.circuit, race.sessionName,
                race.driverCount, race.totalLaps, evCount);
  stage = PLAYING;
  race.ready = true;
  race.status[0] = 0;
  lastClock = INT64_MAX; // first play() starts from the top
}

static void loadSession()
{
  char url[200];
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
  JsonDocument f;
  f["session_key"] = f["date_start"] = f["date_end"] = f["session_name"] = true;
  f["circuit_short_name"] = f["country_name"] = f["circuit_key"] = true;
  race.sessionKey = 0;
  if (failed(get(url, f, onSession), "sessions")) return;
  if (!race.sessionKey) { failed(404, "session"); return; }
  stage = LOAD_DRIVERS;
}

static void loadDrivers()
{
  char url[100];
  snprintf(url, sizeof(url), API "drivers?session_key=%ld", race.sessionKey);
  JsonDocument f;
  f["driver_number"] = f["name_acronym"] = f["team_colour"] = true;
  race.driverCount = 0;
  if (failed(get(url, f, onDriver), "drivers")) return;
  stage = LOAD_FEEDS;
  loadIdx = 0;
}

static void loadFeed()
{
  while (loadIdx < LOAD_COUNT && !(feeds & LOADS[loadIdx].feed)) loadIdx++;
  if (loadIdx == LOAD_COUNT) { startPlaying(); return; }
  const Load &L = LOADS[loadIdx];
  char url[100];
  snprintf(url, sizeof(url), API "%s?session_key=%ld", L.endpoint, race.sessionKey);
  JsonDocument f;
  for (const char *field : L.fields)
    if (field) f[field] = true;
  int before = evCount, code = get(url, f, L.cb);
  if (code != 404 && failed(code, L.what)) // 404 = nothing of this kind in the session, which is fine
  {
    evCount = before; // drop a half-loaded feed; it's fetched again
    return;
  }
  loadIdx++;
}

// Downloads the next window of a streamed feed if it's running short. Returns true if it asked.
static bool streamStep(Feed &s)
{
  if (!(feeds & s.feed)) return false;
  LOCK();
  int64_t clock = race.clockMs;
  if (s.fetchedUntil < clock - 2000) // just started, fell behind, or restarted (play() zeroes it)
  {
    s.head = s.count = 0;
    s.fetchedUntil = clock - 2000;
  }
  int64_t from = s.fetchedUntil;
  bool need = from < clock + s.windowMs && from < race.endMs && s.count < s.cap / 2;
  UNLOCK();
  if (!need) return false;

  int64_t to = from + s.windowMs;
  char a[32], b[32], url[220];
  isoOf(from, a, sizeof(a));
  isoOf(to, b, sizeof(b));
  snprintf(url, sizeof(url), API "%s?session_key=%ld&date>=%s&date<%s", s.endpoint, race.sessionKey, a, b);
  JsonDocument f;
  for (const char *field : s.fields) f[field] = true;
  stagingCount = 0;
  int code = get(url, f, s.cb);
  if (code != 404 && failed(code, s.endpoint)) return true; // 404 = an empty window, carry on
  qsort(staging, stagingCount, sizeof(Ev), byTime);

  LOCK();
  if (s.fetchedUntil == from) // not reset while we were downloading
  {
    int i = 0;
    for (; i < stagingCount && s.count < s.cap; i++) s.buf[(s.head + s.count++) % s.cap] = staging[i];
    s.fetchedUntil = i < stagingCount ? staging[i].t : to; // full: fetch the rest next time
  }
  UNLOCK();
  return true;
}

// One step of downloading: runs in the background task on the ESP32, inside f1Tick() on the PC.
static void loaderStep()
{
  if (millis() < nextReqAt || !af1WifiOk() || !af1TimeOk()) return;
  if (stage == LOAD_SESSION) loadSession();
  else if (stage == LOAD_DRIVERS) loadDrivers();
  else if (stage == LOAD_FEEDS) loadFeed();
  else
  {
    for (Feed &s : streams)
      if (streamStep(s))
      {
        if (millis() >= nextReqAt) nextReqAt = millis() + STREAM_GAP_MS;
        return;
      }
    return;
  }
  if (millis() >= nextReqAt) nextReqAt = millis() + LOAD_GAP_MS;
}

// ---------- Playback ----------
static void news(F1NewsKind kind, int64_t t, int drv, int drv2, int32_t ms, const char *text)
{
  F1News n = {};
  n.t = t;
  n.kind = kind;
  n.drv = drv;
  n.drv2 = drv2;
  n.lap = race.lap;
  n.ms = ms;
  strlcpy(n.text, text, sizeof(n.text));
  f1PushNews(n);
}

static void updateTyre(F1Driver &d)
{
  int lap = d.lap ? d.lap : 1, best = 0;
  for (int i = 0; i < stintCount; i++)
    if (stints[i].drv == d.num && stints[i].lapStart <= lap && stints[i].lapStart >= best)
    {
      best = stints[i].lapStart;
      d.compound = stints[i].compound;
    }
  d.tyreLaps = best && d.lap ? lap - best + 1 : 0; // on the grid: 0 laps on the set
}

// Car number after `key` in a race control message ("CARS 44 (HAM) AND 16" -> 44), 0 if none
static int numberAfter(const char *msg, const char *key)
{
  const char *p = strstr(msg, key);
  return p ? atoi(p + strlen(key)) : 0;
}

static void applyRaceControl(const Rc &r, int64_t t, bool fresh)
{
  const char *m = r.msg;
  F1NewsKind kind;
  int drv = r.drv, drv2 = 0, turn = 0;
  if (!strcmp(r.flag, "CHEQUERED")) // first: "CHEQUERED FLAG" contains "RED FLAG"
  {
    race.track = TRACK_CHEQUERED;
    kind = NEWS_FINISH;
  }
  else if (!strncmp(m, "RED FLAG", 8) || !strcmp(r.flag, "RED"))
  {
    race.track = TRACK_RED;
    kind = NEWS_RED;
  }
  else if (!strcmp(r.category, "SafetyCar"))
  {
    if (strstr(m, "ENDING") || strstr(m, "IN THIS LAP"))
    {
      race.track = TRACK_GREEN; // ponytail: green straight away; good enough for the screens
      kind = NEWS_SC_ENDING;
    }
    else if (strstr(m, "VSC") || strstr(m, "VIRTUAL"))
    {
      race.track = TRACK_VSC;
      kind = NEWS_VSC;
    }
    else
    {
      race.track = TRACK_SC;
      kind = NEWS_SC;
    }
  }
  else if (!strcmp(r.flag, "GREEN") || !strcmp(m, "RACE START"))
  {
    bool resumed = race.track == TRACK_SC || race.track == TRACK_VSC || race.track == TRACK_RED;
    race.track = TRACK_GREEN;
    if (!resumed) return;
    kind = NEWS_GREEN;
  }
  else if ((strstr(m, "COLLISION") || strstr(m, "CRASH") || strstr(m, "STOPPED") || strstr(m, "CONTACT")) &&
           !strstr(m, "STEWARDS"))
  {
    kind = NEWS_CRASH;
    turn = numberAfter(m, "TURN ");
    drv = drv ? drv : numberAfter(m, "CARS ") ? numberAfter(m, "CARS ") : numberAfter(m, "CAR ");
    drv2 = numberAfter(m, " AND ");
  }
  else if (strstr(m, "PENALTY FOR") && !strstr(m, "SERVED"))
  {
    kind = NEWS_PENALTY;
    drv = drv ? drv : numberAfter(m, "CAR ");
  }
  else
    return;
  if (!fresh) return;
  F1News n = {};
  n.t = t;
  n.kind = kind;
  n.drv = goodDriver(drv) ? drv : 0;
  n.drv2 = goodDriver(drv2) ? drv2 : 0;
  n.lap = race.lap;
  n.turn = turn > 0 && turn < 256 ? turn : 0;
  strlcpy(n.text, m, sizeof(n.text));
  f1PushNews(n);
}

static void apply(const Ev &e, bool fresh)
{
  char text[80];
  F1Driver *d = f1Driver(e.drv);
  switch (e.type)
  {
  case EV_RC:
    applyRaceControl(rc[e.v], e.t, fresh);
    return;
  case EV_WX:
    race.hasWeather = true;
    race.airC = wx[e.v].air;
    race.trackC = wx[e.v].track;
    race.humidity = wx[e.v].humidity;
    race.raining = wx[e.v].rain;
    return;
  }
  if (!d) return; // a car that isn't in the driver list
  switch (e.type)
  {
  case EV_POS:
  {
    F1Driver *leader = f1DriverAt(1);
    d->pos = e.v;
    if (e.v == 1 && leader && leader != d && race.lap >= 1 && fresh)
    {
      snprintf(text, sizeof(text), "%s takes the lead from %s", d->code, leader->code);
      news(NEWS_LEAD, e.t, d->num, leader->num, 0, text);
    }
    break;
  }
  case EV_LAP:
    d->lap = e.w;
    updateTyre(*d);
    if (e.w > race.lap)
    {
      if (race.lap == 0)
      {
        for (int i = 0; i < race.driverCount; i++) race.drivers[i].gridPos = race.drivers[i].pos;
        if (fresh) news(NEWS_START, e.t, 0, 0, 0, "Lights out and away we go");
      }
      if (race.track == TRACK_RED || race.track == TRACK_UNKNOWN) race.track = TRACK_GREEN;
      race.lap = e.w;
    }
    break;
  case EV_LAPTIME:
    d->lastLapMs = e.v;
    if (!d->bestLapMs || e.v < d->bestLapMs) d->bestLapMs = e.v;
    if (!race.fastestLapMs || e.v < race.fastestLapMs)
    {
      race.fastestLapMs = e.v;
      race.fastestLapDrv = d->num;
      race.fastestLapNo = e.w;
      snprintf(text, sizeof(text), "Fastest lap: %s %d:%06.3f", d->code, e.v / 60000, (e.v % 60000) / 1000.0);
      if (fresh && race.lap >= 3) news(NEWS_FASTEST, e.t, d->num, 0, e.v, text); // early laps are all "fastest"
    }
    break;
  case EV_PIT:
    d->pitStops++;
    if (!race.fastestPitMs || e.v < race.fastestPitMs)
    {
      race.fastestPitMs = e.v;
      race.fastestPitDrv = d->num;
    }
    snprintf(text, sizeof(text), "%s pits - %.1f s", d->code, e.v / 1000.0);
    if (fresh) news(NEWS_PIT, e.t, d->num, 0, e.v, text);
    break;
  case EV_GAP:
    d->lapsDown = e.v < 0 && e.v > -100 ? -e.v : 0;
    d->gapMs = e.v >= 0 ? e.v : -1;
    d->intervalMs = e.w == 0xFFFF ? -1 : e.w * 10;
    break;
  default: // EV_LOC
  {
    LocHist &h = hist[d->num];
    if (h.n == 4) // drop the oldest
    {
      memmove(h.t, h.t + 1, 3 * sizeof(h.t[0]));
      memmove(h.x, h.x + 1, 3 * sizeof(h.x[0]));
      memmove(h.y, h.y + 1, 3 * sizeof(h.y[0]));
      h.n = 3;
    }
    h.t[h.n] = e.t;
    h.x[h.n] = e.v;
    h.y[h.n++] = (int16_t)e.w;
  }
  }
}

// Car position at time t, between the two samples either side of it
static void locate(F1Driver &d, int64_t t)
{
  LocHist &h = hist[d.num];
  d.hasLocation = h.n > 0;
  if (!h.n) return;
  int i = 0;
  while (i < h.n - 1 && h.t[i + 1] <= t) i++;
  if (i == h.n - 1 || t <= h.t[i])
  {
    d.x = h.x[i];
    d.y = h.y[i];
    return;
  }
  float f = (float)(t - h.t[i]) / (h.t[i + 1] - h.t[i]);
  d.x = h.x[i] + (int16_t)((h.x[i + 1] - h.x[i]) * f);
  d.y = h.y[i] + (int16_t)((h.y[i + 1] - h.y[i]) * f);
}

static void resetRace()
{
  evNext = 0;
  for (int i = 0; i < race.driverCount; i++)
  {
    F1Driver &d = race.drivers[i];
    F1Driver fresh = F1Driver();
    fresh.num = d.num;
    memcpy(fresh.code, d.code, sizeof(fresh.code));
    fresh.colour = d.colour;
    fresh.gapMs = fresh.intervalMs = -1;
    d = fresh;
    updateTyre(d); // starting tyre
  }
  memset(hist, 0, sizeof(hist));
  race.lap = 0;
  race.track = TRACK_UNKNOWN;
  race.fastestLapMs = race.fastestPitMs = 0;
  race.fastestLapDrv = race.fastestLapNo = race.fastestPitDrv = 0;
  race.hasWeather = race.raining = false;
  for (Feed &s : streams) s.head = s.count = 0, s.fetchedUntil = 0;
}

static int64_t span() { return race.endMs - race.startMs; }

static void play()
{
  int64_t clock = race.startMs + (af1NowMs() * speed + offset) % span(); // offset keeps this >= 0
  LOCK();
  if (clock < lastClock) resetRace(); // looped round or restarted: replay from the top
  lastClock = race.clockMs = clock;

  for (; evNext < evCount && ev[evNext].t <= clock; evNext++) apply(ev[evNext], ev[evNext].t > clock - NEWS_FRESH_MS);
  for (Feed &s : streams)
    for (; s.count && s.buf[s.head].t <= clock; s.head = (s.head + 1) % s.cap, s.count--) apply(s.buf[s.head], true);
  UNLOCK();
  for (int i = 0; i < race.driverCount; i++) locate(race.drivers[i], clock - LOC_DELAY_MS);
}

// ---------- Public ----------
#ifdef ARDUINO
static void loaderTask(void *)
{
  for (;;)
  {
    loaderStep();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
#endif

void f1Begin(long sessionKey, int s, uint16_t f)
{
  wantKey = sessionKey;
  speed = s;
  feeds = f;
  offset = 0;
  if (!ev)
  {
    ev = (Ev *)ps_malloc(EV_MAX * sizeof(Ev));
    rc = (Rc *)ps_malloc(RC_MAX * sizeof(Rc));
    for (Feed &st : streams) st.buf = (Ev *)ps_malloc(st.cap * sizeof(Ev));
    staging = (Ev *)ps_malloc(STAGING_MAX * sizeof(Ev));
#ifdef ARDUINO
    lock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(loaderTask, "f1load", 24576, nullptr, 1, nullptr, 0); // TLS + ROM unzip (~11 KB stack)
#endif
  }
  uint32_t newsCount = race.newsCount; // news numbers keep counting up so boards don't re-show old items
  race = F1Race();
  race.newsCount = newsCount;
  evCount = rcCount = wxCount = stintCount = 0;
  resetRace();
  stage = LOAD_SESSION;
  nextReqAt = 0;
}

void f1Tick()
{
  if (stage == PLAYING) play();
#ifndef ARDUINO
  loaderStep(); // PC tests: no background task
#endif
}

// Both keep every board's replay in step only if all boards do the same.
void f1SetSpeed(int s)
{
  if (!race.ready) { speed = s; return; }
  int64_t now = af1NowMs(), at = race.clockMs - race.startMs;
  speed = s;
  offset = at - (now * speed) % span();
}

void f1Restart()
{
  if (!race.ready) return;
  offset = -((af1NowMs() * speed) % span());
}

F1Driver *f1DriverAt(int pos)
{
  for (int i = 0; i < race.driverCount; i++)
    if (race.drivers[i].pos == pos) return &race.drivers[i];
  return nullptr;
}

F1Driver *f1Driver(int num)
{
  for (int i = 0; i < race.driverCount; i++)
    if (race.drivers[i].num == num) return &race.drivers[i];
  return nullptr;
}

// ---------- News ----------
static F1News newsRing[F1_NEWS_KEEP];

void f1PushNews(const F1News &n)
{
  F1News &slot = newsRing[race.newsCount % F1_NEWS_KEEP];
  slot = n;
  slot.seq = race.newsCount++;
}

const F1News *f1News(uint32_t seq)
{
  if (seq >= race.newsCount || seq + F1_NEWS_KEEP < race.newsCount) return nullptr;
  return &newsRing[seq % F1_NEWS_KEEP];
}

const char *f1TrackName(F1Track t)
{
  static const char *names[] = {"", "GREEN", "VIRTUAL SAFETY CAR", "SAFETY CAR", "RED FLAG", "CHEQUERED FLAG"};
  return t <= TRACK_CHEQUERED ? names[t] : "";
}
