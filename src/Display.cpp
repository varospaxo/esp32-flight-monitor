#include "Display.h"
#include "Utils.h"
#include <WiFi.h>
TFT_eSPI tft = TFT_eSPI();
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if ( y >= tft.height() ) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}
void tftClear() {
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
