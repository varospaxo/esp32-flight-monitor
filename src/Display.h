#ifndef DISPLAY_H
#define DISPLAY_H
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
extern TFT_eSPI tft;
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
void tftClear();
void tftHeader(const char* title, uint16_t color);
void drawText(const String& s);
#endif // DISPLAY_H
