// Shared bits for every host test binary: the fake hardware knobs and a tiny CHECK runner.
#pragma once
#include <AF1.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

extern int fake_tz_calls;
extern bool fake_local_ok; // getLocalTime() succeeds

extern int checks, failures;
#define CHECK(cond)                                                                       \
  do                                                                                      \
  {                                                                                       \
    checks++;                                                                             \
    if (!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)
#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_STR(a, b) CHECK(std::string(a) == std::string(b))

static const time_t NOV7 = 1794038709; // Sat 7 Nov 2026 08:05:09 UTC

void resetFakes(); // clock, wifi, HTTP routes, touch back to defaults (wifi connected, time set)
void route(const std::string &match, int code, const std::string &body); // fake OpenF1 reply
