// Stand-in for GFX Library for Arduino: records what was drawn as text lines.
#pragma once
#include <Arduino.h>

enum { BEGIN_WRITE = 1, WRITE_C8_D8, WRITE_C8_BYTES, WRITE_COMMAND_8, END_WRITE, DELAY };
#define GFX_NOT_DEFINED -1
#define RGB565_BLACK 0x0000
#define RGB565_WHITE 0xFFFF
#define RGB565_RED 0xF800
#define RGB565_GREEN 0x07E0
#define RGB565_YELLOW 0xFFE0
#define RGB565_DARKGREY 0x7BEF
#define RGB565_LIGHTGREY 0xC618

struct Arduino_DataBus {};
struct Arduino_ESP32QSPI : Arduino_DataBus
{
  Arduino_ESP32QSPI(int, int, int, int, int, int) {}
};
struct Arduino_GFX {};
struct Arduino_ST77916 : Arduino_GFX
{
  Arduino_ST77916(Arduino_DataBus *, int, int, bool, int, int, int, int, int, int, const uint8_t *, size_t) {}
};

struct DrawnText { int x, y, size; uint16_t colour; std::string s; };
extern bool fake_gfx_begin_ok;

struct Arduino_Canvas
{
  std::vector<DrawnText> texts; // since the last fillScreen
  int flushes = 0, rects = 0;
  int cx = 0, cy = 0, size = 1;
  uint16_t colour = 0;
  Arduino_Canvas(int, int, Arduino_GFX *, int, int, int) {}
  bool begin(int32_t) { return fake_gfx_begin_ok; }
  void fillScreen(uint16_t) { texts.clear(); }
  void setTextSize(int s) { size = s; }
  void setTextColor(uint16_t c) { colour = c; }
  void setCursor(int x, int y) { cx = x, cy = y; }
  void print(const char *s) { texts.push_back({cx, cy, size, colour, s}); }
  void drawFastHLine(int, int, int, uint16_t) {}
  void fillRect(int, int, int, int, uint16_t) { rects++; }
  void flush() { flushes++; }
  bool shows(const std::string &needle) const
  {
    for (auto &t : texts)
      if (t.s.find(needle) != std::string::npos) return true;
    return false;
  }
};
