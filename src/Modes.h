#ifndef MODES_H
#define MODES_H
#include <Arduino.h>
void modeFlight();
void modeAirport();
void modeMap();
void modeWeather();
void modeClock();
void modeSystem();
void modeGIF();
void modeSettings();
void modeNetworkInfo();
void modeCustomText();
int handleSettingsTouch(uint16_t tx, uint16_t ty);
int handleNetworkTouch(uint16_t tx, uint16_t ty);
void closeGIF();
void updateMode();
#endif // MODES_H
