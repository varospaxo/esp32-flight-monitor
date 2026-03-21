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
int    btnPin   = 0; // Default to GPIO 0 (often BOOT button)
String timezone = "Asia/Kolkata";
long   tzOffset = 19800;
String tzAbbr   = "IST";
String dashUser = "admin";
String dashPass = "admin";
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
  doc["wifi_ssid"] = ssid;  doc["wifi_pass"] = pass;
  doc["mode"]      = mode;
  doc["dash_user"] = dashUser; doc["dash_pass"] = dashPass;
  doc["lat"]       = lat;
  doc["lon"]       = lon;
  doc["range_km"]  = range_km;
  doc["units"]     = units;
  doc["f_ground"]  = filterGround;
  doc["f_glider"]  = filterGliders;
  doc["btn_pin"]   = btnPin;
  doc["timezone"]  = timezone;
  doc["tzOffset"]  = tzOffset;
  doc["tzAbbr"]    = tzAbbr;
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
  Log.printf("saveConfig OK: %u bytes  mode=%d\n", written, mode);
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
  if (doc["btn_pin"].is<int>())           btnPin   = doc["btn_pin"].as<int>();
  if (doc["timezone"].is<const char*>())  timezone = doc["timezone"].as<String>();
  if (doc["tzOffset"].is<long>())         tzOffset = doc["tzOffset"].as<long>();
  if (doc["tzAbbr"].is<const char*>())    tzAbbr = doc["tzAbbr"].as<String>();
  xSemaphoreGive(configMutex);
  Log.printf("loadConfig: ssid=%s mode=%d\n", ssid.c_str(), mode);
}
