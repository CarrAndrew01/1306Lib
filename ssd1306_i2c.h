#ifndef SSD1306H
#define SSD1306H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "ssd1306_font.h"


//For future reference, you have to do this because apparently the name gets mangled if you dont cast it to C


extern uint8_t bufferGlobal[1024];

extern "C" void DrawLine(uint8_t *buf, int x0, int y0, int x1, int y1, bool on);
extern "C" void DisplayImage(int posX, int posY, int width, int height, const uint8_t *hex); // one way
extern "C" void InitializeScreen(); // one way
extern "C" void DeleteScreen();
extern "C" void DeleteWindow(int startCol, int endCol, int startPage, int endPage);
extern "C" void UpdateFromGlobal();
extern "C" void calc_render_area_buflen(struct render_area *area);
extern "C" void WriteString(uint8_t *buf,  int16_t x, int16_t y, const char *str,  bool invert);
extern "C" int GetLongestString(const char **text, int length);
extern "C" void WriteStringBlock(uint8_t *buf,  int16_t x, int16_t y, const char *str, int longest, int* counter, bool invert);
extern "C" void SetPixel(uint8_t *buf, int x,int y, bool on);

#endif