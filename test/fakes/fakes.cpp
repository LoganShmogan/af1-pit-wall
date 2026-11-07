// Definitions of the fake hardware state shared by all test binaries.
#include "testing.h"
#include <zlib.h>
#include <miniz.h>

uint32_t fake_millis = 0;
time_t fake_time = 0;
std::map<int, int> fake_pins;
std::string fake_serial;
FakeSerial Serial;
FakeTouchChip fake_touch;
TwoWire Wire;
FakeWiFi WiFi;
bool fake_gfx_begin_ok = true;
std::vector<std::pair<std::string, FakeReply>> fake_routes;
std::vector<std::string> fake_requests;
bool fake_http_begin_ok = true;
bool fake_gzip = false;
int fake_write_error = 0;
std::string fake_accept_encoding;
void (*fake_on_get)() = nullptr;
int fake_tz_calls = 0;
bool fake_local_ok = true;
int checks = 0, failures = 0;

void configTzTime(const char *, const char *, const char *, const char *) { fake_tz_calls++; }
bool getLocalTime(struct tm *info, uint32_t)
{
  if (!fake_local_ok) return false;
  gmtime_r(&fake_time, info); // tests use UTC so expectations don't depend on the host's zone
  return true;
}
// Linked with -Wl,--wrap=time,--wrap=gettimeofday so the code under test sees fake_time.
extern "C" time_t __wrap_time(time_t *t)
{
  if (t) *t = fake_time;
  return fake_time;
}
extern "C" int __wrap_gettimeofday(struct timeval *tv, void *)
{
  int64_t ms = (int64_t)fake_time * 1000 + fake_millis; // NTP time moves on with millis()
  tv->tv_sec = ms / 1000;
  tv->tv_usec = (ms % 1000) * 1000;
  return 0;
}

void resetFakes()
{
  fake_millis = 1000;
  fake_time = NOV7;
  fake_serial.clear();
  fake_routes.clear();
  fake_requests.clear();
  fake_http_begin_ok = true;
  fake_gzip = false;
  fake_write_error = 0;
  fake_on_get = nullptr;
  fake_touch = FakeTouchChip();
  WiFi.st = WL_CONNECTED;
  fake_local_ok = true;
}

void route(const std::string &match, int code, const std::string &body)
{
  fake_routes.push_back({match, {code, body}});
}

// gzip-compress (what OpenF1 sends when asked)
std::string fakeGzip(const std::string &plain)
{
  z_stream z = {};
  deflateInit2(&z, 6, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY); // +16 = gzip wrapper
  std::string out(compressBound(plain.size()) + 32, '\0');
  z.next_in = (Bytef *)plain.data();
  z.avail_in = plain.size();
  z.next_out = (Bytef *)&out[0];
  z.avail_out = out.size();
  deflate(&z, Z_FINISH);
  out.resize(z.total_out);
  deflateEnd(&z);
  return out;
}

// The ESP32 ROM decompressor, done with the PC's zlib
size_t tinfl_decompress_mem_to_mem(void *out, size_t outLen, const void *in, size_t inLen, int)
{
  z_stream z = {};
  inflateInit2(&z, -15); // raw deflate
  z.next_in = (Bytef *)in;
  z.avail_in = inLen;
  z.next_out = (Bytef *)out;
  z.avail_out = outLen;
  int r = inflate(&z, Z_FINISH);
  size_t got = z.total_out;
  inflateEnd(&z);
  return r == Z_STREAM_END ? got : TINFL_DECOMPRESS_MEM_TO_MEM_FAILED;
}
