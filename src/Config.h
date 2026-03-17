#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>

extern String ssid;
extern String pass;
extern float lat;
extern float lon;
extern int range_km;
extern int mode;
extern String timezone;
extern long tzOffset;
extern String dashUser;
extern String dashPass;

void loadConfig();
bool saveConfig();
bool readJson(const char* path, JsonDocument& doc);

#endif // CONFIG_H
