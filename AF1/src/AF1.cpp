// Screen, touch, wifi and clock for the LCDwiki ES3C35P.
// The ST77922 isn't supported directly; it runs through Arduino_ST77916 with
// LCDwiki's own init sequence (proven in Jobs 1 and 2 and the Map Board).
#include "AF1.h"
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy AF1/src/secrets.example.h to AF1/src/secrets.h and put the wifi name and password in it"
#endif

// New Zealand time, daylight saving included
static const char *NZ_TIMEZONE = "NZST-12NZDT,M9.5.0,M4.1.0/3";

#define PIN_CS        10
#define PIN_CLK       12
#define PIN_D0        11
#define PIN_D1        13
#define PIN_D2        14
#define PIN_D3         9
#define PIN_BACKLIGHT 41

#define TOUCH_SDA     38
#define TOUCH_SCL     39
#define TOUCH_INT     47
#define TOUCH_RST     48
#define TOUCH_ADDR    0x55

static const uint8_t st77922_init_operations[] = {
    BEGIN_WRITE,
    WRITE_C8_D8, 0xF1, 0x00,
    WRITE_C8_BYTES, 0x60, 3, 0x00, 0x00, 0x00,
    WRITE_C8_D8, 0x65, 0x80,
    WRITE_C8_D8, 0x79, 0x06,
    WRITE_C8_BYTES, 0x7B, 3, 0x00, 0x08, 0x08,
    WRITE_C8_BYTES, 0x80, 11, 0x55, 0x62, 0x2F, 0x17, 0xF0, 0x52, 0x70, 0xD2, 0x52, 0x62, 0xEA,
    WRITE_C8_BYTES, 0x81, 4, 0x26, 0x52, 0x72, 0x27,
    WRITE_C8_BYTES, 0x84, 2, 0x92, 0x25,
    WRITE_C8_BYTES, 0x87, 6, 0x10, 0x10, 0x58, 0x00, 0x02, 0x3A,
    WRITE_C8_BYTES, 0x88, 15, 0x00, 0x00, 0x2C, 0x10, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x06,
    WRITE_C8_BYTES, 0x89, 3, 0x00, 0x00, 0x00,
    WRITE_C8_BYTES, 0x8A, 11, 0x13, 0x00, 0x2C, 0x00, 0x00, 0x2C, 0x10, 0x10, 0x00, 0x3E, 0x19,
    WRITE_C8_BYTES, 0x8B, 9, 0x15, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x97, 0x8E,
    WRITE_C8_BYTES, 0x8C, 13, 0x1D, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x50, 0x0F, 0x01, 0xC5, 0x12, 0x09,
    WRITE_C8_D8, 0x8D, 0x0C,
    WRITE_C8_BYTES, 0x8E, 6, 0x33, 0x01, 0x0C, 0x13, 0x01, 0x01,
    WRITE_C8_BYTES, 0xB3, 2, 0x00, 0x30,
    WRITE_C8_D8, 0xF1, 0x00,
    WRITE_C8_D8, 0x71, 0xD0,
    WRITE_C8_BYTES, 0x66, 2, 0x02, 0x3F,
    WRITE_C8_BYTES, 0xBE, 3, 0x26, 0x00, 0x9D,
    WRITE_C8_BYTES, 0x70, 12, 0x01, 0xA0, 0x11, 0x40, 0xE0, 0x00, 0x11, 0x69, 0x11, 0x00, 0x00, 0x1A,
    WRITE_C8_BYTES, 0x90, 9, 0x04, 0x04, 0x55, 0x74, 0x00, 0x40, 0x43, 0x27, 0x27,
    WRITE_C8_BYTES, 0x91, 9, 0x04, 0x04, 0x55, 0x75, 0x00, 0x40, 0x42, 0x27, 0x27,
    WRITE_C8_BYTES, 0x92, 10, 0x04, 0x44, 0x55, 0xC0, 0x06, 0x00, 0x07, 0x05, 0x90, 0x27,
    WRITE_C8_BYTES, 0x93, 10, 0x04, 0x43, 0x11, 0x00, 0x00, 0x00, 0x00, 0x05, 0x90, 0x27,
    WRITE_C8_BYTES, 0x94, 6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    WRITE_C8_BYTES, 0x95, 5, 0x96, 0x16, 0x00, 0x00, 0xFF,
    WRITE_C8_BYTES, 0x96, 12, 0x44, 0x53, 0x03, 0x12, 0x23, 0x24, 0x06, 0x05, 0x94, 0x27, 0x00, 0x44,
    WRITE_C8_BYTES, 0x97, 12, 0x44, 0x53, 0x47, 0x56, 0x20, 0x20, 0x02, 0x01, 0x94, 0x27, 0x00, 0x44,
    WRITE_C8_BYTES, 0xBA, 5, 0x55, 0x94, 0x2D, 0x94, 0x27,
    WRITE_C8_BYTES, 0x9A, 7, 0x40, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00,
    WRITE_C8_BYTES, 0x9B, 7, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00,
    WRITE_C8_BYTES, 0x9C, 13, 0x5C, 0x12, 0x00, 0x00, 0x10, 0x12, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00,
    WRITE_C8_BYTES, 0x9D, 8, 0x8A, 0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01,
    WRITE_C8_BYTES, 0x9E, 7, 0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01,
    WRITE_C8_BYTES, 0xB4, 12, 0x1D, 0x1C, 0x1E, 0x0B, 0x14, 0x02, 0x13, 0x09, 0x1E, 0x00, 0x1E, 0x10,
    WRITE_C8_BYTES, 0xB5, 12, 0x1D, 0x1C, 0x1E, 0x0A, 0x15, 0x03, 0x11, 0x08, 0x1E, 0x01, 0x1E, 0x12,
    WRITE_C8_BYTES, 0xB6, 7, 0x77, 0x77, 0x00, 0x0A, 0xFF, 0x0A, 0xFF,
    WRITE_C8_BYTES, 0x86, 14, 0xCD, 0x04, 0xB1, 0x02, 0x58, 0x12, 0x58, 0x0C, 0x13, 0x01, 0xA5, 0x00, 0xA5, 0xA5,
    WRITE_C8_BYTES, 0xB7, 16, 0x07, 0x0A, 0x0E, 0x06, 0x05, 0x03, 0x2B, 0x03, 0x03, 0x42, 0x07, 0x10, 0x10, 0x2E, 0x3F, 0x0D,
    WRITE_C8_BYTES, 0xB8, 16, 0x07, 0x0A, 0x0D, 0x05, 0x05, 0x02, 0x2B, 0x02, 0x03, 0x42, 0x06, 0x10, 0x0F, 0x2E, 0x3F, 0x0D,
    WRITE_C8_BYTES, 0xB9, 2, 0x23, 0x23,
    WRITE_C8_BYTES, 0xBF, 6, 0x10, 0x14, 0x14, 0x0B, 0x0B, 0x0B,
    WRITE_C8_D8, 0xF2, 0x00,
    WRITE_C8_BYTES, 0x73, 5, 0x04, 0xDA, 0x12, 0x54, 0x47,
    WRITE_C8_BYTES, 0x77, 5, 0x6B, 0x5B, 0xFD, 0xC3, 0xC5,
    WRITE_C8_BYTES, 0x7A, 2, 0x15, 0x27,
    WRITE_C8_BYTES, 0x7B, 2, 0x04, 0x57,
    WRITE_C8_BYTES, 0x7E, 2, 0x01, 0x0E,
    WRITE_C8_D8, 0xBF, 0x36,
    WRITE_C8_BYTES, 0xE3, 2, 0x40, 0x40,
    WRITE_C8_D8, 0xF0, 0x00,
    WRITE_C8_D8, 0xD0, 0x00,
    WRITE_C8_BYTES, 0x2A, 4, 0x00, 0x00, 0x01, 0x3F,
    WRITE_C8_BYTES, 0x2B, 4, 0x00, 0x00, 0x01, 0xDF,
    WRITE_COMMAND_8, 0x21,
    WRITE_COMMAND_8, 0x11,
    END_WRITE,
    DELAY, 120,
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,
    WRITE_COMMAND_8, 0x2C,
    WRITE_C8_D8, 0x3A, 0x01,
    WRITE_C8_D8, 0x36, 0x00,
    WRITE_C8_D8, 0x35, 0x01,
    END_WRITE,
    DELAY, 20,
    BEGIN_WRITE,
    END_WRITE};

static Arduino_DataBus *bus = new Arduino_ESP32QSPI(PIN_CS, PIN_CLK, PIN_D0, PIN_D1, PIN_D2, PIN_D3);
static Arduino_GFX *panel = new Arduino_ST77916(bus, GFX_NOT_DEFINED, 0, true, 320, 480, 0, 0, 0, 0,
                                                st77922_init_operations, sizeof(st77922_init_operations));
Arduino_Canvas *gfx = new Arduino_Canvas(320, 480, panel, 0, 0, 1); // rotation 1 = landscape 480x320

// ---------- Display ----------
bool af1Begin()
{
  bool ok = gfx->begin(40000000);
  if (!ok) Serial.println("Screen did not start - check PSRAM = OPI PSRAM");
  pinMode(PIN_BACKLIGHT, OUTPUT);
  af1Backlight(true);

  pinMode(TOUCH_INT, INPUT_PULLUP);
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(10);
  digitalWrite(TOUCH_RST, HIGH);
  delay(100);
  Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000);
  return ok;
}

void af1Backlight(bool on) { digitalWrite(PIN_BACKLIGHT, on ? HIGH : LOW); }

uint16_t af1Rgb(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void af1Text(int x, int y, int size, uint16_t colour, const char *s)
{
  gfx->setTextSize(size);
  gfx->setTextColor(colour);
  gfx->setCursor(x, y);
  gfx->print(s);
}

void af1Centred(const char *s, int y, int size, uint16_t colour)
{
  af1Text((480 - (int)strlen(s) * 6 * size) / 2, y, size, colour, s);
}

// ---------- Touch (same chip protocol as Job 2) ----------
static bool touchReg(uint16_t reg, uint8_t *data, size_t len)
{
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; i++) data[i] = Wire.read();
  return true;
}

bool af1TouchRaw(int &x, int &y)
{
  uint8_t info;
  if (!touchReg(0x0010, &info, 1) || !(info & 0x08)) return false;
  uint8_t p[7];
  if (!touchReg(0x0014, p, 7) || !(p[0] & 0x80)) return false;
  int nx = ((p[0] & 0x3F) << 8) | p[1];
  int ny = ((p[2] & 0x3F) << 8) | p[3];
  x = ny; // touch reports portrait; turn it to landscape (Job 2 confirmed no flips needed)
  y = 319 - nx;
  x = constrain(x, 0, 479);
  y = constrain(y, 0, 319);
  return true;
}

TouchEvent af1Touch()
{
  static bool down = false, held = false;
  static int sx, sy, lx, ly;
  static uint32_t downAt;
  TouchEvent e = {T_NONE, 0, 0};
  int x, y;
  if (af1TouchRaw(x, y))
  {
    if (!down)
    {
      down = true;
      held = false;
      sx = lx = x;
      sy = ly = y;
      downAt = millis();
    }
    lx = x;
    ly = y;
    if (!held && millis() - downAt > 700 && abs(lx - sx) < 20 && abs(ly - sy) < 20)
    {
      held = true;
      e = {T_HOLD, sx, sy};
    }
    return e;
  }
  if (!down) return e;
  down = false;
  if (held) return e;
  int dx = lx - sx, dy = ly - sy;
  if (abs(dx) > 60 && abs(dx) >= abs(dy)) e.kind = dx < 0 ? T_SWIPE_LEFT : T_SWIPE_RIGHT;
  else if (abs(dy) > 60) e.kind = dy < 0 ? T_SWIPE_UP : T_SWIPE_DOWN;
  else e.kind = T_TAP;
  e.x = sx;
  e.y = sy;
  return e;
}

// ---------- Wifi + clock ----------
static uint32_t wifiTryAt = 0;
static bool sntpStarted = false;

void af1NetBegin(const char *hostname)
{
  WiFi.mode(WIFI_STA); // ponytail: switch to WIFI_AP_STA when ESP-NOW arrives (Job 15)
  WiFi.setHostname(hostname);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  // At full power one of the boards was never let in (router: "auth expired", reason 2);
  // at 8.5 dBm it joins at once. The boards sit near the router, so nothing is lost.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  wifiTryAt = millis();
  Serial.printf("Wifi: joining %s\n", WIFI_NAME);
}

void af1NetTick()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    wifiTryAt = millis();
    if (!sntpStarted)
    {
      Serial.printf("Wifi OK, %s\n", WiFi.localIP().toString().c_str());
      configTzTime(NZ_TIMEZONE, "nz.pool.ntp.org", "pool.ntp.org", "time.google.com");
      sntpStarted = true;
    }
    return;
  }
  // Auto-reconnect usually fixes drops; if not, start over every 20 s.
  if (millis() - wifiTryAt > 20000)
  {
    Serial.println("Wifi: still down, trying again");
    WiFi.disconnect();
    WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
    wifiTryAt = millis();
  }
}

bool af1WifiOk() { return WiFi.status() == WL_CONNECTED; }
bool af1TimeOk() { return time(nullptr) > 1700000000; }

int64_t af1NowMs()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
