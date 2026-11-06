// The 23 season maps in round order, with the transform from OpenF1 track x/y to screen
// pixels: px = ax*x + bx*y + cx, py = ay*x + by*y + cy. Made by tools/build_maps.py.
// Each map is two PNGs, the left and right 240 px of the screen.
#pragma once
#include <Arduino.h>

struct MapInfo
{
  int circuitKey, round;
  const char *name;
  const uint8_t *left, *right;
  uint32_t leftSize, rightSize;
  float ax, bx, cx, ay, by, cy;
};

extern const uint8_t map01_left[], map01_right[];
extern const uint8_t map02_left[], map02_right[];
extern const uint8_t map03_left[], map03_right[];
extern const uint8_t map04_left[], map04_right[];
extern const uint8_t map05_left[], map05_right[];
extern const uint8_t map06_left[], map06_right[];
extern const uint8_t map07_left[], map07_right[];
extern const uint8_t map08_left[], map08_right[];
extern const uint8_t map09_left[], map09_right[];
extern const uint8_t map10_left[], map10_right[];
extern const uint8_t map11_left[], map11_right[];
extern const uint8_t map12_left[], map12_right[];
extern const uint8_t map13_left[], map13_right[];
extern const uint8_t map14_left[], map14_right[];
extern const uint8_t map15_left[], map15_right[];
extern const uint8_t map16_left[], map16_right[];
extern const uint8_t map17_left[], map17_right[];
extern const uint8_t map18_left[], map18_right[];
extern const uint8_t map19_left[], map19_right[];
extern const uint8_t map20_left[], map20_right[];
extern const uint8_t map21_left[], map21_right[];
extern const uint8_t map22_left[], map22_right[];
extern const uint8_t map23_left[], map23_right[];

static const MapInfo MAPS[] = {
    {10, 1, "Albert Park Circuit", map01_left, map01_right, 49620, 48639, 0.008556389f, -0.02154181f, 293.3062f, -0.02130675f, -0.008618205f, 172.9978f},
    {49, 2, "Shanghai International Circuit", map02_left, map02_right, 53375, 44001, 0.01428695f, -0.02280215f, 258.6591f, -0.02295655f, -0.01441894f, 115.5814f},
    {46, 3, "Suzuka Circuit", map03_left, map03_right, 38148, 52754, 0.02247860f, 0.0003188821f, 328.6836f, 0.0004714102f, -0.02215856f, 115.1710f},
    {151, 4, "Miami International Autodrome", map04_left, map04_right, 45765, 46301, 0.02932441f, 0.0006228471f, 149.8224f, 0.0001920178f, -0.02933774f, 100.8618f},
    {23, 5, "Circuit Gilles Villeneuve", map05_left, map05_right, 43504, 43797, -0.005632675f, -0.02441700f, 425.8253f, -0.02464210f, 0.006076199f, 149.3602f},
    {22, 6, "Circuit De Monaco", map06_left, map06_right, 49780, 61827, -0.03353766f, -0.02377023f, 4.962712f, -0.02302100f, 0.03325478f, 234.7678f},
    {15, 7, "Circuit De Barcelona-Catalunya", map07_left, map07_right, 78280, 79023, -0.01052593f, -0.03205034f, 160.6869f, -0.03175288f, 0.01046093f, 114.1712f},
    {19, 8, "Red Bull Ring", map08_left, map08_right, 62543, 51452, 0.02969222f, 8.833542e-05f, 297.7083f, 4.198891e-05f, -0.02932767f, 205.9996f},
    {2, 9, "Silverstone Circuit", map09_left, map09_right, 83503, 71398, -0.004643209f, -0.02391548f, 364.3466f, -0.02389457f, 0.004453877f, 208.4027f},
    {7, 10, "Spa-Francorchamps", map10_left, map10_right, 65141, 73021, -0.002194713f, -0.01849963f, 138.4552f, -0.01851198f, 0.002125796f, 218.6118f},
    {4, 11, "Hungaroring", map11_left, map11_right, 55180, 57742, -0.01807318f, -0.01915998f, 328.1101f, -0.01910888f, 0.01798566f, 84.07249f},
    {55, 12, "Circuit Zandvoort", map12_left, map12_right, 95125, 76057, 0.02825590f, 0.0007116117f, 128.7385f, 0.0006758361f, -0.02823670f, 227.5371f},
    {39, 13, "Autodromo Nazionale Monza", map13_left, map13_right, 46397, 25176, -0.0008707448f, -0.01962796f, 342.1434f, -0.01957945f, 0.0009297654f, 245.9597f},
    {153, 14, "Madring", map14_left, map14_right, 58875, 51411, 0.006135311f, -0.02318180f, 392.9234f, -0.02331281f, -0.006060483f, 210.2276f},
    {144, 15, "Baku City Circuit", map15_left, map15_right, 45895, 61324, -0.01974776f, -0.003689195f, 94.02269f, -0.003584665f, 0.01925320f, 207.3213f},
    {12, 16, "Sepang International Circuit", map16_left, map16_right, 83115, 89134, 0.02466571f, -9.674199e-06f, 223.2007f, -4.016651e-05f, -0.02454794f, 136.0713f},
    {61, 17, "Marina Bay Street Circuit", map17_left, map17_right, 62419, 67014, -0.03027492f, -0.004590377f, 50.70019f, -0.004486561f, 0.03063435f, 141.6652f},
    {9, 18, "Circuit Of The Americas", map18_left, map18_right, 58108, 81883, -0.01891187f, -0.01722278f, 422.6170f, -0.01722503f, 0.01863394f, 209.3319f},
    {65, 19, "Autodromo Hermanos Rodriguez", map19_left, map19_right, 61020, 57242, 0.02064162f, -0.01662895f, 103.6960f, -0.01666468f, -0.02045055f, 222.5183f},
    {14, 20, "Interlagos", map20_left, map20_right, 49626, 45743, -0.009351908f, -0.03603439f, 104.6266f, -0.03577175f, 0.009443999f, 140.9584f},
    {152, 21, "Las Vegas Strip Circuit", map21_left, map21_right, 51624, 41965, -0.0007454458f, -0.01984253f, 394.0889f, -0.01972314f, 0.0007102888f, 114.2515f},
    {150, 22, "Lusail International Circuit", map22_left, map22_right, 86025, 74105, 0.01146758f, -0.02212166f, 282.4048f, -0.02209023f, -0.01134675f, 231.9581f},
    {70, 23, "Yas Marina Circuit", map23_left, map23_right, 44233, 64222, -0.003286662f, -0.02639164f, 336.0124f, -0.02626699f, 0.003163393f, 209.6640f},
};
static const int MAP_COUNT = sizeof(MAPS) / sizeof(MAPS[0]);
