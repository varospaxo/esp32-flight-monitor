#ifndef DISPLAY_H
#define DISPLAY_H
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
extern TFT_eSPI tft;
extern int lastDrawnMode;
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
void tftClear();
void tftHeader(const char* title, uint16_t color);
void drawText(const String& s);
// Touch/Swipe handling
struct TouchPoint {
  uint16_t x;
  uint16_t y;
};
enum SwipeDirection {
  SWIPE_NONE,
  SWIPE_LEFT,
  SWIPE_RIGHT,
  SWIPE_UP,
  SWIPE_DOWN
};
SwipeDirection detectSwipe(TouchPoint start, TouchPoint end);
// UI drawing
void drawCheckbox(int x, int y, bool checked, const char* label);
void drawButton(int x, int y, int w, int h, bool pressed, const char* label);
#endif // DISPLAY_H
