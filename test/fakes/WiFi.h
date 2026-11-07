// Stand-in for the ESP32 WiFi class.
#pragma once
#include <Arduino.h>

#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
#define WIFI_STA 1
#define WIFI_POWER_8_5dBm 34

struct FakeIP
{
  String toString() const { return String("192.168.1.50"); }
};

struct FakeWiFi
{
  int st = WL_DISCONNECTED;
  int begins = 0, disconnects = 0;
  std::string hostname;
  void mode(int) {}
  void setHostname(const char *h) { hostname = h; }
  void setAutoReconnect(bool) {}
  void setSleep(bool) {}
  int txPower = 0;
  void setTxPower(int p) { txPower = p; }
  void begin(const char *, const char *) { begins++; }
  void disconnect() { disconnects++; }
  int status() { return st; }
  FakeIP localIP() { return FakeIP(); }
};
extern FakeWiFi WiFi;
