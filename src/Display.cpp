#include "Display.h"
#include "Utils.h"
#include <WiFi.h>
TFT_eSPI tft = TFT_eSPI();
int lastDrawnMode = -1;
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if ( y >= tft.height() ) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}
void tftClear() {
  lastDrawnMode = -1;
  tft.fillScreen(TFT_BLACK);
}
void tftHeader(const char* title, uint16_t color) {
  tft.fillRect(0, 0, 320, 22, color);
  tft.setTextColor(TFT_BLACK, color);
  tft.setTextSize(1);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(title, 4, 11);
  String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "AP:192.168.4.1";
  tft.setTextDatum(MR_DATUM);
  tft.drawString(ip, 316, 11);
  tft.setTextDatum(TL_DATUM);
}
void drawText(const String& s) {
  tftClear();
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);
  tft.println(s);
  setPreview(s);
}

// Swipe detection with minimum movement threshold
SwipeDirection detectSwipe(TouchPoint start, TouchPoint end) {
  const int MIN_SWIPE = 12;
  int dx = end.x - start.x;
  int dy = end.y - start.y;
  int ax = abs(dx), ay = abs(dy);

  if (ax < MIN_SWIPE && ay < MIN_SWIPE) return SWIPE_NONE;

  if (ax > ay) return dx < 0 ? SWIPE_RIGHT : SWIPE_LEFT;
  else         return dy < 0 ? SWIPE_UP    : SWIPE_DOWN;
}

// Draw a checkbox with label
void drawCheckbox(int x, int y, bool checked, const char* label) {
  const int boxSize = 12;
  const int boxSpacing = 4;
  
  // Draw box outline
  tft.drawRect(x, y, boxSize, boxSize, checked ? TFT_GREEN : TFT_DARKGREY);
  
  // Draw checkmark if checked
  if (checked) {
    tft.drawLine(x + 3, y + 6, x + 5, y + 9, TFT_GREEN);
    tft.drawLine(x + 5, y + 9, x + 9, y + 3, TFT_GREEN);
    tft.drawLine(x + 4, y + 6, x + 5, y + 9, TFT_GREEN);
    tft.drawLine(x + 5, y + 9, x + 9, y + 4, TFT_GREEN);
  }
  
  // Draw label
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(x + boxSize + boxSpacing, y + 2);
  tft.print(label);
}

// Draw a button with optional pressed state
void drawButton(int x, int y, int w, int h, bool pressed, const char* label) {
  uint16_t borderColor = pressed ? TFT_GREEN   : TFT_DARKGREY;
  uint16_t fillColor   = pressed ? 0x0460      : TFT_BLACK;   // dark green fill when on
  uint16_t textColor   = pressed ? TFT_GREEN   : TFT_LIGHTGREY;

  tft.fillRect(x + 1, y + 1, w - 2, h - 2, fillColor);
  tft.drawRect(x, y, w, h, borderColor);

  // Use size 2 for bigger buttons, size 1 for small ones
  tft.setTextSize(h >= 34 ? 2 : 1);
  tft.setTextColor(textColor, fillColor);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, x + w / 2, y + h / 2);
  tft.setTextDatum(TL_DATUM);
}

