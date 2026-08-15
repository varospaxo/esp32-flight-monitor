#include "WebServerHelper.h"
#include "Globals.h"
#include "Config.h"
#include "FlightData.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
AsyncWebServer server(80);
bool checkAuth(AsyncWebServerRequest* req) {
  String u, p;
  if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    Log.println("checkAuth: mutex timeout!");
    req->send(503, "text/plain", "busy");
    return false;
  }
  u = dashUser; p = dashPass;
  xSemaphoreGive(configMutex);
  if (!req->authenticate(u.c_str(), p.c_str())) { req->requestAuthentication(); return false; }
  return true;
}
void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    req->send(LittleFS, "/index.html", "text/html");
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(LittleFS, "/style.css", "text/css");
  });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(LittleFS, "/script.js", "application/javascript");
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(204);
  });
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    String previewSnap;
    xSemaphoreTake(previewMutex, pdMS_TO_TICKS(50));
    previewSnap = preview;
    xSemaphoreGive(previewMutex);
    JsonDocument doc;
    doc["ip"]         = WiFi.localIP().toString();
    doc["rssi"]       = WiFi.RSSI();
    doc["preview"]    = previewSnap;
    doc["heap"]       = ESP.getFreeHeap();
    doc["ssid"]       = WiFi.SSID();
    doc["channel"]    = WiFi.channel();
    doc["adsb_ok"]    = (bool)adsbOk;
    doc["weather_ok"] = (bool)weatherOk;
    doc["uptime"]     = millis() / 1000;
    doc["updating"]   = (bool)updating;
    xSemaphoreTake(configMutex, portMAX_DELAY);
    doc["mode"]       = mode;
    doc["range_km"]   = range_km;
    doc["timezone"]   = timezone;
    doc["tzAbbr"]     = tzAbbr;
    doc["lat"]        = lat;
    doc["lon"]        = lon;
    xSemaphoreGive(configMutex);
    String r; serializeJson(doc, r);
    req->send(200, "application/json", r);
  });
  server.on("/api/mode", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    if (!req->hasParam("m")) { req->send(400, "text/plain", "missing m"); return; }
    int m = req->getParam("m")->value().toInt();
    if (m < 1 || m > 10 || m == 8 || m == 9) { req->send(400, "text/plain", "m out of range"); return; }
    xSemaphoreTake(configMutex, portMAX_DELAY);
    mode = m;
    saveConfig();
    xSemaphoreGive(configMutex);
    lastModeCycle = millis();
    Log.printf("/api/mode -> mode=%d\n", m);
    req->send(200, "application/json", "{\"mode\":" + String(m) + "}");
  });
  server.on("/api/upload-gif", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      if (!checkAuth(req)) return;
      if (req->_tempObject) {
        req->send(400, "application/json", "{\"error\":\"File exceeds 512KB limit\"}");
        return;
      }
      req->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"GIF uploaded successfully\"}");
    },
    [](AsyncWebServerRequest* req, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (index == 0) {
        Log.printf("Upload GIF start: %s\n", filename.c_str());
        gifUploadPending = true;
        delay(150); // Give loop() ample time to close the file if open
        req->_tempFile = LittleFS.open("/custom.gif_tmp", "w");
        req->_tempObject = (void*)0;
      }
      if (index + len > 524288) {
        Log.println("Upload GIF: file exceeds 512KB limit");
        if (req->_tempFile) {
          req->_tempFile.close();
        }
        LittleFS.remove("/custom.gif_tmp");
        req->_tempObject = (void*)1;
        gifUploadPending = false;
        return;
      }
      if (req->_tempFile && (size_t)req->_tempObject == 0) {
        req->_tempFile.write(data, len);
      }
      if (final) {
        if (req->_tempFile) {
          req->_tempFile.close();
        }
        if ((size_t)req->_tempObject == 0) {
          LittleFS.remove("/custom.gif");
          LittleFS.rename("/custom.gif_tmp", "/custom.gif");
          Log.printf("Upload GIF complete: %u bytes\n", index + len);
        }
        gifUploadPending = false;
      }
    }
  );
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    JsonDocument doc;
    xSemaphoreTake(configMutex, portMAX_DELAY);
    doc["lat"]        = lat;
    doc["lon"]        = lon;
    doc["range"]      = range_km;
    doc["mode"]       = mode;
    doc["timezone"]   = timezone;
    doc["tzOffset"]   = tzOffset;
    doc["tzAbbr"]     = tzAbbr;
    doc["units"]      = units;
    doc["f_ground"]   = filterGround;
    doc["f_glider"]   = filterGliders;
    doc["auto_cycle"] = autoCycle;
    doc["cycle_mins"] = cycleMins;
    doc["cycle_modes"] = cycleModes;
    doc["btn_pin"]    = btnPin;
    doc["custom_text"]    = customText;
    doc["text_style"]     = customTextStyle;
    doc["text_direction"] = customTextDirection;
    xSemaphoreGive(configMutex);
    String r; serializeJson(doc, r);
    req->send(200, "application/json", r);
  });
  server.on("/api/config/save", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    savePending = true;
    bool hasLat    = req->hasParam("lat", true);
    bool hasLon    = req->hasParam("lon", true);
    bool hasRange  = req->hasParam("range", true);
    bool hasTz     = req->hasParam("timezone", true);
    bool hasOffset = req->hasParam("tzOffset", true);
    bool hasAbbr   = req->hasParam("tzAbbr", true);
    xSemaphoreTake(configMutex, portMAX_DELAY);
    if (hasLat) lat = strtof(req->getParam("lat", true)->value().c_str(), nullptr);
    if (hasLon) lon = strtof(req->getParam("lon", true)->value().c_str(), nullptr);
    if (hasLat || hasLon) {
      inferred_apt_code = "---"; inferred_apt_name = ""; inferred_apt_city = "";
      inferred_apt_icao = ""; inferred_apt_lat = 0; inferred_apt_lon = 0;
    }
    if (hasRange)  range_km = req->getParam("range", true)->value().toInt();
    if (hasTz)     timezone = req->getParam("timezone", true)->value();
    if (hasOffset) tzOffset = req->getParam("tzOffset", true)->value().toInt();
    if (hasAbbr)   tzAbbr = req->getParam("tzAbbr", true)->value();
    if (req->hasParam("units", true))      units = req->getParam("units", true)->value().toInt();
    if (req->hasParam("f_ground", true))   filterGround = req->getParam("f_ground", true)->value() == "true";
    if (req->hasParam("f_glider", true))   filterGliders = req->getParam("f_glider", true)->value() == "true";
    if (req->hasParam("auto_cycle", true)) autoCycle = req->getParam("auto_cycle", true)->value() == "true";
    if (req->hasParam("cycle_mins", true)) {
      int cm = req->getParam("cycle_mins", true)->value().toInt();
      cycleMins = cm > 0 ? cm : 1;
    }
    if (req->hasParam("cycle_modes", true)) {
      String normalized = normalizeCycleModes(req->getParam("cycle_modes", true)->value());
      if (normalized.length() > 0) cycleModes = normalized; // ignore attempts to leave zero modes enabled
    }
    if (req->hasParam("btn_pin", true))    btnPin = req->getParam("btn_pin", true)->value().toInt();
    if (req->hasParam("custom_text", true)) {
      String txt = req->getParam("custom_text", true)->value();
      if (txt.length() > 200) txt = txt.substring(0, 200); // cap to protect flash/heap
      customText = txt;
    }
    if (req->hasParam("text_style", true)) {
      int st = req->getParam("text_style", true)->value().toInt();
      customTextStyle = (st == 1) ? 1 : 0;
    }
    if (req->hasParam("text_direction", true)) {
      int dir = req->getParam("text_direction", true)->value().toInt();
      customTextDirection = (dir == 1) ? 1 : 0;
    }
    
    lastModeCycle = millis();

    configTime(tzOffset, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", "", 1);
    tzset();
    
    bool ok = saveConfig();
    float r_lat = lat, r_lon = lon; int r_range = range_km; String r_tz = timezone; long r_off = tzOffset; String r_abbr = tzAbbr;
    int r_units = units; bool r_f_ground = filterGround, r_f_glider = filterGliders;
    bool r_auto_cycle = autoCycle; int r_cycle_mins = cycleMins; String r_cycle_modes = cycleModes; int r_btn_pin = btnPin;
    String r_custom_text = customText; int r_text_style = customTextStyle; int r_text_direction = customTextDirection;
    xSemaphoreGive(configMutex);
    savePending = false;
    if (!ok) { req->send(500, "application/json", "{\"error\":\"Failed to write config\"}"); return; }
    JsonDocument doc;
    doc["lat"] = r_lat; doc["lon"] = r_lon; doc["range"] = r_range; doc["timezone"] = r_tz; doc["tzOffset"] = r_off; doc["tzAbbr"] = r_abbr;
    doc["units"] = r_units; doc["f_ground"] = r_f_ground; doc["f_glider"] = r_f_glider;
    doc["auto_cycle"] = r_auto_cycle; doc["cycle_mins"] = r_cycle_mins; doc["cycle_modes"] = r_cycle_modes; doc["btn_pin"] = r_btn_pin;
    doc["custom_text"] = r_custom_text; doc["text_style"] = r_text_style; doc["text_direction"] = r_text_direction;
    String r; serializeJson(doc, r);
    req->send(200, "application/json", r);
  });
  server.on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    if (!req->hasParam("ssid") || !req->hasParam("pass")) { req->send(400, "text/plain", "missing ssid or pass"); return; }
    String newSsid = req->getParam("ssid")->value();
    String newPass = req->getParam("pass")->value();
    xSemaphoreTake(configMutex, portMAX_DELAY);
    ssid = newSsid; pass = newPass;
    bool ok = saveConfig();
    xSemaphoreGive(configMutex);
    req->send(ok ? 200 : 500, "text/plain", ok ? "saved, rebooting" : "save failed");
    if (ok) { delay(500); ESP.restart(); }
  });
  // Destructive: requires an explicit confirm=yes body param, in addition to the browser-side confirm dialog
  server.on("/api/forget-wifi", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    if (!req->hasParam("confirm", true) || req->getParam("confirm", true)->value() != "yes") {
      req->send(400, "text/plain", "confirmation required"); return;
    }
    xSemaphoreTake(configMutex, portMAX_DELAY);
    ssid = ""; pass = "";
    bool ok = saveConfig();
    xSemaphoreGive(configMutex);
    Log.println("/api/forget-wifi: credentials cleared");
    req->send(ok ? 200 : 500, "text/plain", ok ? "wifi forgotten, rebooting" : "save failed");
    if (ok) { delay(500); ESP.restart(); }
  });
  // Destructive: wipes /config.json entirely, restoring compiled-in defaults on next boot
  server.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    if (!req->hasParam("confirm", true) || req->getParam("confirm", true)->value() != "yes") {
      req->send(400, "text/plain", "confirmation required"); return;
    }
    Log.println("/api/factory-reset: wiping config");
    LittleFS.remove("/config.json");
    req->send(200, "text/plain", "factory reset, rebooting");
    delay(500);
    ESP.restart();
  });
  server.on("/api/credentials", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    if (!req->hasParam("user") || !req->hasParam("pass")) { req->send(400, "text/plain", "missing user or pass"); return; }
    String newUser = req->getParam("user")->value();
    String newPass = req->getParam("pass")->value();
    xSemaphoreTake(configMutex, portMAX_DELAY);
    dashUser = newUser; dashPass = newPass;
    bool ok = saveConfig();
    xSemaphoreGive(configMutex);
    req->send(ok ? 200 : 500, "text/plain", ok ? "ok" : "save failed");
  });
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    req->send(200, "text/plain", "rebooting");
    delay(500);
    ESP.restart();
  });
  ElegantOTA.begin(&server);
  server.begin();
  Log.println("Web server started");
}
