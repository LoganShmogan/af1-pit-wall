// Host-side stand-in for the ESP32 Arduino core: just what AF1 uses, with knobs for tests.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include <ctime>
#include <string>
#include <map>
#include <vector>
#include <sys/time.h>

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT_PULLUP 2

struct String : std::string
{
  String() {}
  String(const char *s) : std::string(s) {}
  String(const std::string &s) : std::string(s) {}
};

// ---- test knobs ----
extern uint32_t fake_millis;
extern time_t fake_time;                // what time() returns
extern std::map<int, int> fake_pins;    // last digitalWrite per pin
extern std::string fake_serial;         // everything printed to Serial

inline uint32_t millis() { return fake_millis; }
inline void delay(uint32_t ms) { fake_millis += ms; }
inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int v) { fake_pins[pin] = v; }
inline void *ps_malloc(size_t n) { return malloc(n); }
inline void *ps_realloc(void *p, size_t n) { return realloc(p, n); }
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t *)(p))
template <typename T> T constrain(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; }

// esp32-hal-time
void configTzTime(const char *tz, const char *s1, const char *s2, const char *s3);
bool getLocalTime(struct tm *info, uint32_t ms = 5000);

struct FakeSerial
{
  void begin(int) {}
  int printf(const char *fmt, ...)
  {
    char b[512];
    va_list a;
    va_start(a, fmt);
    int n = vsnprintf(b, sizeof(b), fmt, a);
    va_end(a);
    fake_serial += b;
    return n;
  }
  void println(const char *s) { fake_serial += s, fake_serial += "\n"; }
};
extern FakeSerial Serial;

// Needed by ArduinoJson's Arduino-stream support (not used by AF1 itself)
struct Print
{
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *b, size_t n)
  {
    for (size_t i = 0; i < n; i++) write(b[i]);
    return n;
  }
};
struct Printable
{
  virtual size_t printTo(Print &) const = 0;
};

// Arduino Stream over an in-memory string (what HTTPClient::getStream() hands out).
class Stream
{
public:
  std::string data;
  size_t at = 0;
  virtual ~Stream() {}
  virtual size_t write(const uint8_t *, size_t n) { return n; } // overridden by sinks
  size_t readBytes(char *buf, size_t n)
  {
    size_t k = std::min(n, data.size() - at);
    memcpy(buf, data.data() + at, k);
    at += k;
    return k;
  }
  bool find(const char *t)
  {
    size_t p = data.find(t, at);
    at = p == std::string::npos ? data.size() : p + strlen(t);
    return p != std::string::npos;
  }
  // true if `t` comes before `stop` (Arduino semantics: consumes up to and including whichever is hit)
  bool findUntil(const char *t, const char *stop)
  {
    size_t p = data.find(t, at), q = data.find(stop, at);
    if (p != std::string::npos && (q == std::string::npos || p < q)) { at = p + strlen(t); return true; }
    at = q == std::string::npos ? data.size() : q + strlen(stop);
    return false;
  }
};

// PNGdec reads its tables with memcpy_P (flash) on the ESP32
#define memcpy_P memcpy
