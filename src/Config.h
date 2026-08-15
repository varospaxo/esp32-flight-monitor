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
extern int units;         // 0: Imperial, 1: Metric
extern bool filterGround;
extern bool filterGliders;
extern bool autoCycle;
extern int  cycleMins;
extern String cycleModes;
extern int btnPin;        // GPIO pin for mode button
extern String timezone;
extern long tzOffset;
extern String tzAbbr;
extern String dashUser;
extern String dashPass;
extern String customText;
extern int customTextStyle;     // 0: Static, 1: Marquee
extern int customTextDirection; // 0: Scroll Left, 1: Scroll Right
extern bool telnetEnabled;      // Telnet logging server toggle (port 23)
// Modes eligible for auto-cycle / cycle-mode buttons (8 and 9 are Settings/Network, not cycleable)
#define NUM_CYCLE_MODES 8
extern const int CYCLE_MODE_LIST[NUM_CYCLE_MODES];
void loadConfig();
bool saveConfig();
bool readJson(const char* path, JsonDocument& doc);
String normalizeCycleModes(const String& raw);
int getNextCycleMode(int currentMode, const String& rawModes);
bool cycleModeEnabled(const String& rawModes, int m);
#endif // CONFIG_H
