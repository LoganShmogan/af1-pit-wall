// Stand-in for GFX Library for Arduino: records text and counts shapes so tests can check
// what a screen shows. Has a real 480x320 framebuffer (landscape) for code that copies it.
#pragma once
#include <Arduino.h>

enum { BEGIN_WRITE = 1, WRITE_C8_D8, WRITE_C8_BYTES, WRITE_COMMAND_8, END_WRITE, DELAY };
#define GFX_NOT_DEFINED -1
#define RGB565_BLACK 0x0000
#define RGB565_NAVY 0x000F
#define RGB565_DARKGREEN 0x03E0
#define RGB565_MAROON 0x7800
#define RGB565_PURPLE 0x780F
#define RGB565_OLIVE 0x7BE0
#define RGB565_LIGHTGREY 0xC618
#define RGB565_DARKGREY 0x7BEF
#define RGB565_BLUE 0x001F
#define RGB565_GREEN 0x07E0
#define RGB565_CYAN 0x07FF
#define RGB565_RED 0xF800
#define RGB565_MAGENTA 0xF81F
#define RGB565_YELLOW 0xFFE0
#define RGB565_WHITE 0xFFFF
#define RGB565_ORANGE 0xFD20
#define RGB565_GREENYELLOW 0xAFE5
#define RGB565_PINK 0xFC18

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
  // Shape counters (since the start; tests compare before/after)
  int flushes = 0, rects = 0, circles = 0, lines = 0, pixels = 0, bitmaps = 0, triangles = 0;
  int cx = 0, cy = 0, size = 1;
  uint16_t colour = 0, bg = 0;
  uint16_t fb[480 * 320] = {}; // landscape framebuffer: fb[y * 480 + x]

  Arduino_Canvas(int, int, Arduino_GFX *, int, int, int) {}
  bool begin(int32_t = 0) { return fake_gfx_begin_ok; }
  int16_t width() { return 480; }
  int16_t height() { return 320; }
  uint16_t *getFramebuffer() { return fb; }
  void flush() { flushes++; }

  void fillScreen(uint16_t c)
  {
    texts.clear();
    for (auto &p : fb) p = c;
  }
  void drawPixel(int16_t x, int16_t y, uint16_t c)
  {
    pixels++;
    if (x >= 0 && x < 480 && y >= 0 && y < 320) fb[y * 480 + x] = c;
  }
  void drawLine(int16_t, int16_t, int16_t, int16_t, uint16_t) { lines++; }
  void drawFastHLine(int16_t, int16_t, int16_t, uint16_t) { lines++; }
  void drawFastVLine(int16_t, int16_t, int16_t, uint16_t) { lines++; }
  void drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t) { rects++; }
  void fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t) { rects++; }
  void drawRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) { rects++; }
  void fillRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) { rects++; }
  void drawCircle(int16_t, int16_t, int16_t, uint16_t) { circles++; }
  void fillCircle(int16_t, int16_t, int16_t, uint16_t) { circles++; }
  void drawTriangle(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) { triangles++; }
  void fillTriangle(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) { triangles++; }
  void draw16bitRGBBitmap(int16_t, int16_t, uint16_t *, int16_t, int16_t) { bitmaps++; }

  // Text: built-in 6x8 font scaled by size
  void setTextSize(uint8_t s) { size = s; }
  void setTextColor(uint16_t c) { colour = c; }
  void setTextColor(uint16_t c, uint16_t b) { colour = c, bg = b; }
  void setCursor(int16_t x, int16_t y) { cx = x, cy = y; }
  int16_t getCursorX() { return cx; }
  int16_t getCursorY() { return cy; }
  void print(const char *s)
  {
    texts.push_back({cx, cy, size, colour, s});
    cx += (int)strlen(s) * 6 * size;
  }
  void print(int v) { print(std::to_string(v).c_str()); }
  void println(const char *s)
  {
    print(s);
    cx = 0, cy += 8 * size;
  }

  // Test helpers
  bool shows(const std::string &needle) const
  {
    for (auto &t : texts)
      if (t.s.find(needle) != std::string::npos) return true;
    return false;
  }
  std::string all() const // every text item on screen, " | " separated (handy in failure output)
  {
    std::string r;
    for (auto &t : texts) r += t.s + " | ";
    return r;
  }
};
