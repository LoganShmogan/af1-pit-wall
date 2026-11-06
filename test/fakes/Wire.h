// Stand-in for the I2C bus with a fake touch chip behind it.
#pragma once
#include <Arduino.h>

struct FakeTouchChip
{
  uint8_t status = 0;          // register 0x0010
  uint8_t point[7] = {};       // register 0x0014
  bool nack = false;           // endTransmission fails
  uint16_t failReg = 0xFFFF;   // endTransmission fails for this register only
  bool shortRead = false;      // requestFrom returns fewer bytes
  // Finger at landscape (x, y)
  void press(int x, int y)
  {
    int nx = 319 - y, ny = x;  // landscape -> chip's portrait numbers
    status = 0x08;
    point[0] = 0x80 | (nx >> 8);
    point[1] = nx & 0xFF;
    point[2] = ny >> 8;
    point[3] = ny & 0xFF;
  }
  void release() { status = 0; }
};
extern FakeTouchChip fake_touch;

struct TwoWire
{
  uint16_t reg = 0;
  int got = 0;
  size_t pos = 0;
  void begin(int, int, uint32_t) {}
  void beginTransmission(uint8_t) { reg = 0, got = 0; }
  void write(uint8_t b) { reg = (reg << 8) | b, got++; }
  uint8_t endTransmission(bool) { return fake_touch.nack || reg == fake_touch.failReg ? 2 : 0; }
  uint8_t requestFrom(uint8_t, uint8_t len)
  {
    pos = 0;
    return fake_touch.shortRead ? len - 1 : len;
  }
  uint8_t read() { return reg == 0x0010 ? fake_touch.status : fake_touch.point[pos++]; }
};
extern TwoWire Wire;
