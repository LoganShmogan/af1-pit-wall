// Definitions of the fake hardware state shared by all test binaries.
#include "testing.h"

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
  fake_on_get = nullptr;
  fake_touch = FakeTouchChip();
  WiFi.st = WL_CONNECTED;
  fake_local_ok = true;
}

void route(const std::string &match, int code, const std::string &body)
{
  fake_routes.push_back({match, {code, body}});
}
