#include "Config.h"
#include "Globals.h"
#include <LittleFS.h>
String ssid     = "";
String pass     = "";
float  lat      = 0;
float  lon      = 0;
int    range_km = 10;
int    mode     = 1;
int    units    = 0; // 0: Imperial, 1: Metric
bool   filterGround  = false;
bool   filterGliders = false;
bool   autoCycle     = false;
int    cycleMins     = 1;
String cycleModes     = "1,2,3,4,5,6,7";
int    btnPin   = 0; // Default to GPIO 0 (often BOOT button)
String timezone = "Asia/Kolkata";
long   tzOffset = 19800;
String tzAbbr   = "IST";
String dashUser = "admin";
String dashPass = "admin";
String customText          = "";
int    customTextStyle     = 0; // 0: Static, 1: Marquee
int    customTextDirection = 0; // 0: Scroll Left, 1: Scroll Right
const int CYCLE_MODE_LIST[NUM_CYCLE_MODES] = {1, 2, 3, 4, 5, 6, 7, 10};
bool readJson(const char* path, JsonDocument& doc) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) { Log.printf("readJson: parse err in %s: %s\n", path, err.c_str()); return false; }
  return true;
}
bool saveConfig() {
  JsonDocument doc;
  doc["wifi_ssid"]  = ssid;  doc["wifi_pass"] = pass;
  doc["mode"]       = mode;
  doc["dash_user"]  = dashUser; doc["dash_pass"] = dashPass;
  doc["lat"]        = lat;
  doc["lon"]        = lon;
  doc["range_km"]   = range_km;
  doc["units"]      = units;
  doc["f_ground"]   = filterGround;
  doc["f_glider"]   = filterGliders;
  doc["auto_cycle"] = autoCycle;
  doc["cycle_mins"] = cycleMins;
  doc["cycle_modes"] = cycleModes;
  doc["btn_pin"]    = btnPin;
  doc["timezone"]   = timezone;
  doc["tzOffset"]   = tzOffset;
  doc["tzAbbr"]     = tzAbbr;
  doc["custom_text"]     = customText;
  doc["text_style"]      = customTextStyle;
  doc["text_direction"]  = customTextDirection;
  const char* tmpPath = "/config_tmp.json";
  const char* dstPath = "/config.json";
  File f = LittleFS.open(tmpPath, "w");
  if (!f) { Log.println("saveConfig: cannot open tmp file"); return false; }
  size_t written = serializeJson(doc, f);
  f.close();
  if (written == 0) {
    Log.println("saveConfig: 0 bytes written");
    LittleFS.remove(tmpPath);
    return false;
  }
  LittleFS.remove(dstPath);
  if (!LittleFS.rename(tmpPath, dstPath)) {
    Log.println("saveConfig: rename failed");
    LittleFS.remove(tmpPath);
    return false;
  }
  Log.printf("saveConfig OK: %u bytes  mode=%d autoCycle=%d cycleMins=%d\n", written, mode, autoCycle, cycleMins);
  return true;
}
void loadConfig() {
  JsonDocument doc;
  bool ok = readJson("/config.json", doc);
  if (!ok) {
    Log.println("loadConfig: no config file found, using defaults");
    return;
  }
  xSemaphoreTake(configMutex, portMAX_DELAY);
  if (doc["wifi_ssid"].is<const char*>()) ssid     = doc["wifi_ssid"].as<String>();
  if (doc["wifi_pass"].is<const char*>()) pass     = doc["wifi_pass"].as<String>();
  if (doc["mode"].is<int>())              mode     = doc["mode"].as<int>();
  if (doc["dash_user"].is<const char*>()) dashUser = doc["dash_user"].as<String>();
  if (doc["dash_pass"].is<const char*>()) dashPass = doc["dash_pass"].as<String>();
  if (doc["lat"].is<float>())             lat      = doc["lat"].as<float>();
  if (doc["lon"].is<float>())             lon      = doc["lon"].as<float>();
  if (doc["range_km"].is<int>())          range_km = doc["range_km"].as<int>();
  if (doc["units"].is<int>())             units    = doc["units"].as<int>();
  if (doc["f_ground"].is<bool>())         filterGround = doc["f_ground"].as<bool>();
  if (doc["f_glider"].is<bool>())         filterGliders = doc["f_glider"].as<bool>();
  if (doc["auto_cycle"].is<bool>())       autoCycle = doc["auto_cycle"].as<bool>();
  if (doc["cycle_mins"].is<int>())        cycleMins = doc["cycle_mins"].as<int>();
  if (doc["cycle_modes"].is<const char*>()) cycleModes = normalizeCycleModes(doc["cycle_modes"].as<String>());
  if (doc["btn_pin"].is<int>())           btnPin   = doc["btn_pin"].as<int>();
  if (doc["timezone"].is<const char*>())  timezone = doc["timezone"].as<String>();
  if (doc["tzOffset"].is<long>())         tzOffset = doc["tzOffset"].as<long>();
  if (doc["tzAbbr"].is<const char*>())    tzAbbr = doc["tzAbbr"].as<String>();
  if (doc["custom_text"].is<const char*>())    customText = doc["custom_text"].as<String>();
  if (doc["text_style"].is<int>())             customTextStyle = doc["text_style"].as<int>();
  if (doc["text_direction"].is<int>())         customTextDirection = doc["text_direction"].as<int>();
  xSemaphoreGive(configMutex);
  Log.printf("loadConfig: ssid=%s mode=%d autoCycle=%d cycleMins=%d cycleModes=%s\n", ssid.c_str(), mode, autoCycle, cycleMins, cycleModes.c_str());
}

static bool isValidCycleMode(int m) {
  for (int i = 0; i < NUM_CYCLE_MODES; i++) if (CYCLE_MODE_LIST[i] == m) return true;
  return false;
}

// Token-exact membership check — plain String::indexOf() is unsafe here since
// mode "1" is a textual substring of mode "10" in a comma-separated list.
bool cycleModeEnabled(const String& rawModes, int m) {
  int start = 0;
  int len = rawModes.length();
  for (int i = 0; i <= len; ++i) {
    if (i == len || rawModes.charAt(i) == ',') {
      if (rawModes.substring(start, i).toInt() == m) return true;
      start = i + 1;
    }
  }
  return false;
}

String normalizeCycleModes(const String& raw) {
  String modes = raw;
  modes.replace(" ", "");
  String result;
  bool seen[11] = {false};
  int start = 0;
  for (int i = 0; i <= modes.length(); ++i) {
    if (i == modes.length() || modes.charAt(i) == ',') {
      String token = modes.substring(start, i);
      start = i + 1;
      if (token.length() == 0) continue;
      int m = token.toInt();
      if (m >= 1 && m <= 10 && isValidCycleMode(m) && !seen[m]) {
        if (result.length() > 0) result += ",";
        result += String(m);
        seen[m] = true;
      }
    }
  }
  return result.length() > 0 ? result : String("1,2,3,4,5,6,7");
}

int getNextCycleMode(int currentMode, const String& rawModes) {
  String modes = normalizeCycleModes(rawModes);
  if (modes.length() == 0) return CYCLE_MODE_LIST[0];
  int selectedModes[NUM_CYCLE_MODES];
  int count = 0;
  int start = 0;
  for (int i = 0; i <= modes.length(); ++i) {
    if (i == modes.length() || modes.charAt(i) == ',') {
      String token = modes.substring(start, i);
      start = i + 1;
      if (token.length() == 0) continue;
      selectedModes[count++] = token.toInt();
      if (count >= NUM_CYCLE_MODES) break;
    }
  }
  if (count == 0) return CYCLE_MODE_LIST[0];
  for (int i = 0; i < count; ++i) {
    if (selectedModes[i] == currentMode) {
      return (i + 1 < count) ? selectedModes[i + 1] : selectedModes[0];
    }
  }
  return selectedModes[0];
}
