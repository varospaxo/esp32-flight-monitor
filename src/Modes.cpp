#include "Modes.h"
#include "Globals.h"
#include "Config.h"
#include "Display.h"
#include "Utils.h"
#include "FlightData.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

void modeFlight() {
  if (WiFi.status() != WL_CONNECTED) { drawText("NO WIFI"); return; }

  float c_lat, c_lon; int c_range;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_lat = lat; c_lon = lon; c_range = range_km;
  xSemaphoreGive(configMutex);

  String api = "https://opendata.adsb.fi/api/v3/lat/" + String(c_lat, 4)
             + "/lon/" + String(c_lon, 4)
             + "/dist/" + String(c_range * 0.54f, 1);

  HTTPClient http; http.setTimeout(8000); http.begin(api);
  int code = http.GET();
  if (code != 200) { drawText("FLIGHT ERR " + String(code)); http.end(); return; }
  String body = http.getString(); http.end();
  adsbOk = true; lastSuccess = millis();

  JsonDocument doc;
  if (deserializeJson(doc, body)) { drawText("FLIGHT PARSE ERR"); return; }
  JsonArray ac = doc["ac"].as<JsonArray>();
  if (ac.size() == 0) { drawText("NO AIRCRAFT\nIN RANGE"); return; }

  struct AcInfo { String flight; int alt; float dist; float spd; String reg; float hdg; int vspd; String type; String country; };
  AcInfo sorted[3]; float dists[3] = {9999,9999,9999}; int count = 0;

  bool snap_fGround, snap_fGlider;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  snap_fGround = filterGround; snap_fGlider = filterGliders;
  xSemaphoreGive(configMutex);

  for (JsonObject a : ac) {
    if (!a["alt_baro"].is<int>()) continue;
    int alt = a["alt_baro"].as<int>();

    if (snap_fGround && alt <= 0) continue;
    String cat = a["category"] | "";
    if (snap_fGlider && (cat == "A4" || cat == "C0")) continue;

    float d    = a["dst"]    | 9999.0f;
    float spd  = a["gs"]     | 0.0f;
    float hdg  = a["track"]  | -1.0f;
    int vspd   = a["baro_rate"] | 0;
    String fl  = a["flight"] | "???"; fl.trim();
    String reg = a["r"]      | "";
    String country = a["trc"] | ""; 
    for (int i = 0; i < 3; i++) {
      if (d < dists[i]) {
        for (int j = 2; j > i; j--) { dists[j] = dists[j-1]; sorted[j] = sorted[j-1]; }
        dists[i] = d; sorted[i] = {fl, alt, d, spd, reg, hdg, vspd, "", country};
        if (count < 3) count++;
        break;
      }
    }
  }

  String airline, route, acType;
  if (sorted[0].flight.length() > 2) {
    HTTPClient http2; http2.setTimeout(5000);
    http2.begin("https://api.adsbdb.com/v0/callsign/" + sorted[0].flight);
    if (http2.GET() == 200) {
      JsonDocument db;
      if (!deserializeJson(db, http2.getString())) {
        JsonObject fr = db["response"]["flightroute"];
        if (!fr.isNull()) {
          JsonObject al = fr["airline"];
          if (!al.isNull()) airline = al["name"] | "";
          JsonObject orig = fr["origin"], dest = fr["destination"];
          if (!orig.isNull() && !dest.isNull())
            route = String(orig["iata_code"] | "???") + " > " + String(dest["iata_code"] | "???");
        }
        JsonObject acObj = db["response"]["aircraft"];
        if (!acObj.isNull()) {
          String mfr = acObj["manufacturer"] | "";
          String typ = acObj["type"] | "";
          if (mfr.length() && typ.length()) acType = mfr + " " + typ;
          else acType = typ;
        }
      }
    }
    http2.end();
  }

  tftClear(); tftHeader(" FLIGHT RADAR", TFT_CYAN);
  int y = 28;

  String icao = "";
  for (int i = 0; i < (int)sorted[0].flight.length(); i++) {
    char c = sorted[0].flight.charAt(i);
    if (c >= 'A' && c <= 'Z') icao += c;
    if (icao.length() == 3) break;
  }
  
  if (icao.length() == 3) {
    WiFiClientSecure clientLogo;
    clientLogo.setInsecure();
    HTTPClient httpLogo; httpLogo.setTimeout(5000);
    String logoUrl = "https://content.airhex.com/content/logos/airlines_" + icao + "_200_200_s.jpg";
    httpLogo.begin(clientLogo, logoUrl);
    if (httpLogo.GET() == 200) {
      int logoSize = 52;
      int lx = 320 - logoSize - 6;
      int ly = 24;
      tft.fillRoundRect(lx - 4, ly - 4, logoSize + 8, logoSize + 8, 6, tft.color565(24, 24, 24));
      String payload = httpLogo.getString();
      TJpgDec.setJpgScale(4);
      TJpgDec.drawJpg(lx + 1, ly + 1, (const uint8_t*)payload.c_str(), payload.length());
    }
    httpLogo.end();
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(2);
  tft.setCursor(4, y); tft.print(sorted[0].flight); y += 20;
  tft.setTextSize(1);
  if (airline.length()) { tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setCursor(4,y); tft.print(airline); y+=12; }
  if (route.length())   { tft.setTextColor(TFT_GREEN,  TFT_BLACK); tft.setCursor(4,y); tft.print(route);   y+=12; }
  if (acType.length())  { tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK); tft.setCursor(4,y); tft.print(acType); y+=12; }
  y += 4;

  int snap_units;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  snap_units = units;
  xSemaphoreGive(configMutex);

  float dispAlt = sorted[0].alt, dispDist = sorted[0].dist, dispSpd = sorted[0].spd, dispVSpd = sorted[0].vspd;
  const char* uAlt = "ft", *uDist = "nm", *uSpd = "kts", *uVSpd = "fpm";
  
  if (snap_units == 1) { // Metric
    dispAlt *= 0.3048f; uAlt = "m";
    dispDist *= 1.852f; uDist = "km";
    dispSpd *= 1.852f;  uSpd = "km/h";
    dispVSpd *= 0.3048f; uVSpd = "mpm";
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4,y); tft.printf("ALT  %.0f %s", dispAlt, uAlt);            y+=12;
  tft.setCursor(4,y); tft.printf("DIST %.1f %s",  dispDist, uDist);          y+=12;
  tft.setCursor(4,y); tft.printf("SPD  %.0f %s", dispSpd, uSpd);           y+=12;
  tft.setCursor(4,y); tft.printf("V/S  %.0f %s", dispVSpd, uVSpd);          y+=12;

  if (sorted[0].hdg >= 0) { tft.setCursor(4,y); tft.printf("HDG  %.0f %s", sorted[0].hdg, headingArrow(sorted[0].hdg)); y+=12; }
  
  if (sorted[0].reg.length()) { 
    tft.setCursor(4,y); tft.printf("REG  %s", sorted[0].reg.c_str()); 
    if (sorted[0].country.length() == 2) {
      String flagUrl = "https://flagcdn.com/w40/" + sorted[0].country + ".jpg";
      HTTPClient fhttp; 
      WiFiClientSecure fclient; fclient.setInsecure();
      fhttp.begin(fclient, flagUrl);
      if (fhttp.GET() == 200) {
        String fpayload = fhttp.getString();
        TJpgDec.setJpgScale(1); 
        TJpgDec.drawJpg(tft.getCursorX() + 6, y - 2, (const uint8_t*)fpayload.c_str(), fpayload.length());
      }
      fhttp.end();
    }
    y+=12; 
  }
  y+=8; tft.drawFastHLine(4,y,312,TFT_DARKGREY); y+=4;
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  for (int i = 1; i < count; i++) {
    float sAlt = sorted[i].alt, sDist = sorted[i].dist;
    if (snap_units == 1) { sAlt *= 0.3048f; sDist *= 1.852f; }
    tft.setCursor(4,y); tft.printf("%s  %.0f%s  %.1f%s", sorted[i].flight.c_str(), sAlt, uAlt, sDist, uDist); y+=12;
  }

  String txt = "FLIGHT RADAR\n" + sorted[0].flight;
  if (airline.length()) txt += "\n" + airline;
  if (route.length())   txt += "\n" + route;
  if (acType.length())  txt += "\n" + acType;
  txt += "\nALT  " + String(dispAlt, 0) + " " + uAlt;
  txt += "\nDIST " + String(dispDist, 1) + " " + uDist;
  txt += "\nSPD  " + String((int)dispSpd) + " " + uSpd;
  txt += "\nV/S  " + String((int)dispVSpd) + " " + uVSpd;
  if (sorted[0].hdg >= 0) txt += "\nHDG  " + String((int)sorted[0].hdg) + " " + headingArrow(sorted[0].hdg);
  if (sorted[0].reg.length()) txt += "\nREG  " + sorted[0].reg;
  for (int i = 1; i < count; i++) {
    float sAlt = sorted[i].alt, sDist = sorted[i].dist;
    if (snap_units == 1) { sAlt *= 0.3048f; sDist *= 1.852f; }
    txt += "\n" + sorted[i].flight + "  " + String(sAlt, 0) + uAlt + "  " + String(sDist, 1) + uDist;
  }
  setPreview(txt);
}

void modeAirport() {
  if (WiFi.status() != WL_CONNECTED) { drawText("NO WIFI"); return; }
  float c_lat, c_lon;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_lat = lat; c_lon = lon;
  xSemaphoreGive(configMutex);
  String api = "https://opendata.adsb.fi/api/v3/lat/" + String(c_lat,4) + "/lon/" + String(c_lon,4) + "/dist/50";
  HTTPClient http; http.setTimeout(8000); http.begin(api);
  int code = http.GET();
  if (code != 200) { drawText("AIRPORT ERR " + String(code)); http.end(); return; }
  String body = http.getString(); http.end();
  adsbOk = true; lastSuccess = millis();
  JsonDocument doc;
  if (deserializeJson(doc, body)) { drawText("PARSE ERR"); return; }
  JsonArray ac = doc["ac"].as<JsonArray>();
  struct Arrival { String flight; int alt; float dist; };
  Arrival arrivals[8]; int arrCount = 0;
  for (JsonObject a : ac) {
    if (!a["alt_baro"].is<int>()) continue;
    int alt = a["alt_baro"].as<int>();
    if (alt <= 0) continue;
    int baro_rate = a["baro_rate"] | 0;
    if (alt < 10000 && baro_rate < -200) {
      String fl = a["flight"] | "???"; fl.trim();
      float d = a["dst"] | 999.0f;
      if (arrCount < 8) {
        int pos = arrCount;
        for (int i = 0; i < arrCount; i++) if (alt < arrivals[i].alt) { pos = i; break; }
        for (int j = arrCount; j > pos; j--) arrivals[j] = arrivals[j-1];
        arrivals[pos] = {fl, alt, d}; arrCount++;
      }
    }
  }
  inferAirport(ac);
  String airport = inferred_apt_code == "---" ? "LOCAL" : inferred_apt_code;
  tftClear(); tftHeader((" " + airport + " ARRIVALS").c_str(), TFT_ORANGE);
  int y = 28; tft.setTextSize(1);
  if (inferred_apt_name.length() > 0) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK); 
    tft.setCursor(4, y); tft.print(inferred_apt_name); y += 12;
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(4, y); tft.print(inferred_apt_icao + "  " + inferred_apt_city); y += 12;
    tft.setCursor(4, y); tft.printf("%.4f, %.4f", inferred_apt_lat, inferred_apt_lon); y += 16;
  } else { y += 12; }
  if (arrCount == 0) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setCursor(4,y); tft.print("No arrivals detected");
  } else {
    for (int i = 0; i < min(arrCount, 5); i++) {
      tft.setTextColor(TFT_WHITE,    TFT_BLACK); tft.setCursor(4,   y); tft.print(arrivals[i].flight);
      tft.setTextColor(TFT_GREEN,    TFT_BLACK); tft.setCursor(100, y); tft.printf("%d ft", arrivals[i].alt);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK); tft.setCursor(220, y); tft.printf("%.1f nm", arrivals[i].dist);
      y += 14;
    }
  }
  String coords = String(inferred_apt_lat, 4) + "," + String(inferred_apt_lon, 4);
  String txt = "AIRPORT MONITOR\n" + airport + "\n" + inferred_apt_name + "|" + inferred_apt_city + "|" + inferred_apt_icao + "|" + coords + "\n";
  for (int i = 0; i < min(arrCount,6); i++)
    txt += arrivals[i].flight + "|" + String(arrivals[i].alt) + " ft|" + String(arrivals[i].dist,1) + " nm\n";
  if (arrCount == 0) txt += "No arrivals detected";
  setPreview(txt);
}

void modeMap() {
  if (WiFi.status() != WL_CONNECTED) { drawText("NO WIFI"); return; }
  float c_lat, c_lon; int c_range;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_lat = lat; c_lon = lon; c_range = range_km;
  xSemaphoreGive(configMutex);
  String api = "https://opendata.adsb.fi/api/v3/lat/" + String(c_lat,4) + "/lon/" + String(c_lon,4) + "/dist/" + String(c_range * 0.54f, 1);
  HTTPClient http; http.setTimeout(8000); http.begin(api);
  int code = http.GET();
  if (code != 200) { drawText("MAP ERR " + String(code)); http.end(); return; }
  String body = http.getString(); http.end();
  adsbOk = true; lastSuccess = millis();
  JsonDocument doc;
  if (deserializeJson(doc, body)) { drawText("MAP PARSE ERR"); return; }
  JsonArray ac = doc["ac"].as<JsonArray>();
  inferAirport(ac);
  tftClear();
  const int cx = 160, cy = 130, maxR = 100;
  tft.drawCircle(cx, cy, maxR,     TFT_DARKGREY);
  tft.drawCircle(cx, cy, maxR / 2, TFT_DARKGREY);
  tft.drawCircle(cx, cy, 3,        TFT_WHITE);
  tft.drawFastHLine(cx - maxR, cy, maxR * 2, TFT_DARKGREY);
  tft.drawFastVLine(cx, cy - maxR, maxR * 2, TFT_DARKGREY);
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setTextSize(1);
  tft.setCursor(4, 2); tft.printf("RADAR  %dkm range", c_range);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(cx + maxR + 4, cy - 4); tft.printf("%dkm", c_range);
  tft.setCursor(cx + maxR/2 - 8, cy + 4); tft.printf("%d", c_range/2);
  float rangeNm = c_range * 0.54f;
  int plotCount = 0;
  String txt = "RADAR MAP\n";
  for (JsonObject a : ac) {
    if (plotCount >= 15) break;
    if (!a["alt_baro"].is<int>()) continue;
    if (a["alt_baro"].as<int>() <= 0) continue;
    float d     = a["dst"]    | 9999.0f;
    float acLat = a["lat"]    | 0.0f;
    float acLon = a["lon"]    | 0.0f;
    String fl   = a["flight"] | "?"; fl.trim();
    if (d > rangeNm || acLat == 0) continue;
    float dx = (acLon - c_lon) * cos(radians(c_lat));
    float dy = (acLat - c_lat);
    float maxDeg = c_range / 111.0f;
    if (maxDeg == 0) continue;
    int px = constrain(cx + (int)((dx / maxDeg) * maxR), 10, 310);
    int py = constrain(cy - (int)((dy / maxDeg) * maxR), 20, 230);
    tft.fillCircle(px, py, 3, TFT_GREEN);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(px + 5, py - 3); tft.print(fl);
    float bearingRad = atan2((acLon - c_lon) * cos(radians(c_lat)), acLat - c_lat);
    int bearing = ((int)(degrees(bearingRad) + 360)) % 360;
    txt += fl + " " + String(d,1) + "nm " + String(bearing) + "\xC2\xB0\n";
    plotCount++;
  }
  if (inferred_apt_lat != 0) {
    float apt_d = haversine(c_lat, c_lon, inferred_apt_lat, inferred_apt_lon) / 1.852f;
    if (apt_d <= rangeNm) {
      float apt_dx = (inferred_apt_lon - c_lon) * cos(radians(c_lat));
      float apt_dy = (inferred_apt_lat - c_lat);
      float maxDeg = c_range / 111.0f;
      if (maxDeg > 0) {
        int apx = constrain(cx + (int)((apt_dx / maxDeg) * maxR), 10, 310);
        int apy = constrain(cy - (int)((apt_dy / maxDeg) * maxR), 20, 230);
        tft.fillCircle(apx, apy, 4, TFT_RED);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(apx + 5, apy - 4); tft.print(inferred_apt_code);
        float bearingRad = atan2((inferred_apt_lon - c_lon) * cos(radians(c_lat)), inferred_apt_lat - c_lat);
        int bearing = ((int)(degrees(bearingRad) + 360)) % 360;
        txt += "[APT] " + inferred_apt_code + " " + String(apt_d, 1) + "nm " + String(bearing) + "\xC2\xB0\n";
      }
    }
  }
  if (plotCount == 0) txt += "No aircraft in range";
  setPreview(txt);
}

void modeWeather() {
  if (WiFi.status() != WL_CONNECTED) { drawText("NO WIFI"); return; }
  float c_lat, c_lon;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_lat = lat; c_lon = lon;
  xSemaphoreGive(configMutex);
  String url = "https://api.open-meteo.com/v1/forecast?latitude="  + String(c_lat,4) + "&longitude=" + String(c_lon,4) + "&current=temperature_2m,relative_humidity_2m,uv_index,precipitation_probability,wind_speed_10m";
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(8000); http.begin(client, url);
  float temp = 0; int humidity = 0, uv = 0, rain = 0; float wind = 0;
  if (http.GET() == 200) {
    String body = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, body)) {
      JsonObject c = doc["current"].as<JsonObject>();
      temp     = c["temperature_2m"]            | 0.0f;
      humidity = c["relative_humidity_2m"]      | 0;
      uv       = c["uv_index"]                  | 0;
      rain     = c["precipitation_probability"] | 0;
      wind     = c["wind_speed_10m"]          | 0.0f;
      weatherOk = true; lastSuccess = millis();
    }
  }
  http.end();
  int aqi = -1;
  WiFiClientSecure client2; client2.setInsecure();
  HTTPClient http2; http2.setTimeout(5000);
  http2.begin(client2, "https://air-quality-api.open-meteo.com/v1/air-quality?latitude="  + String(c_lat,4) + "&longitude=" + String(c_lon,4) + "&current=us_aqi");
  if (http2.GET() == 200) {
    String body2 = http2.getString();
    JsonDocument doc2;
    if (!deserializeJson(doc2, body2)) {
      aqi = doc2["current"]["us_aqi"] | -1;
    }
  }
  http2.end();
  tftClear(); tftHeader(" WEATHER", 0x07E0);
  int y = 30; tft.setTextSize(1); tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4,y); tft.printf("Temperature  %.1f C", temp);    y+=16;
  tft.setCursor(4,y); tft.printf("Humidity     %d%%",  humidity);  y+=16;
  tft.setCursor(4,y); tft.printf("Rain Prob    %d%%",  rain);      y+=16;
  tft.setCursor(4,y); tft.printf("UV Index     %d",    uv);        y+=16;
  tft.setCursor(4,y); tft.printf("Wind Speed   %.1f kph", wind); y+=16;
  if (aqi >= 0) {
    uint16_t aqiColor = aqi > 150 ? TFT_RED : aqi > 100 ? TFT_ORANGE : aqi > 50 ? TFT_YELLOW : TFT_GREEN;
    tft.setTextColor(aqiColor, TFT_BLACK);
    tft.setCursor(4,y); tft.printf("AQI          %d (%s)", aqi, aqiLabel(aqi).c_str());
  }
  String txt = "WEATHER\n";
  txt += "Temperature  " + String(temp,1) + " C\n";
  txt += "Humidity     " + String(humidity) + "%\n";
  txt += "Rain Prob    " + String(rain) + "%\n";
  txt += "UV Index     " + String(uv) + "\n";
  if (aqi >= 0) txt += "AQI          " + String(aqi) + " (" + aqiLabel(aqi) + ")";
  setPreview(txt);
}

void modeClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { drawText("NTP SYNC..."); return; }
  String c_timezone;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_timezone = timezone;
  xSemaphoreGive(configMutex);
  tftClear(); tftHeader(" CLOCK", TFT_MAGENTA);
  char timeBuf[9], dateBuf[20];
  strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S",    &timeinfo);
  strftime(dateBuf, sizeof(dateBuf), "%a, %d %b %Y",&timeinfo);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN,     TFT_BLACK); tft.setTextSize(4); tft.drawString(timeBuf,           160, 85);
  tft.setTextColor(TFT_WHITE,    TFT_BLACK); tft.setTextSize(2); tft.drawString(dateBuf,           160,135);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK); tft.setTextSize(1); tft.drawString(c_timezone.c_str(),160,165);
  tft.setTextDatum(TL_DATUM);
  setPreview("CLOCK\n" + String(timeBuf) + "\n" + String(dateBuf) + "\n" + c_timezone);
}

void modeSystem() {
  tftClear(); tftHeader(" SYSTEM MONITOR", TFT_BLUE);
  int y = 28; tft.setTextSize(1);
  int rssi = WiFi.RSSI();
  int bars = rssi > -50 ? 4 : rssi > -60 ? 3 : rssi > -70 ? 2 : rssi > -80 ? 1 : 0;
  unsigned long ago = (millis() - lastSuccess) / 1000;
  int c_mode;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_mode = mode;
  xSemaphoreGive(configMutex);
  bool snap_adsb = adsbOk, snap_wx = weatherOk;
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setCursor(4,y); tft.print("-- WiFi --"); y+=14;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4,y); tft.printf("SSID: %s", WiFi.SSID().c_str()); y+=12;
  tft.setCursor(4,y); tft.printf("Signal: %d dBm", rssi);
  for (int i = 0; i < 4; i++) {
    tft.setTextColor(i < bars ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(200 + i*8, y); tft.print("|");
  }
  y+=12;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4,y); tft.printf("Channel: %d", WiFi.channel()); y+=12;
  tft.setCursor(4,y); tft.printf("IP: %s", WiFi.localIP().toString().c_str()); y+=16;
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setCursor(4,y); tft.print("-- API Status --"); y+=14;
  tft.setCursor(4,y); tft.setTextColor(snap_adsb ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.printf("ADSB API: %s", snap_adsb ? "OK" : "FAIL"); y+=12;
  tft.setCursor(4,y); tft.setTextColor(snap_wx ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.printf("Weather API: %s", snap_wx ? "OK" : "FAIL"); y+=12;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4,y); tft.printf("Last update: %lus ago", ago); y+=16;
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setCursor(4,y); tft.print("-- System --"); y+=14;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4,y); tft.printf("Heap: %d KB", ESP.getFreeHeap()/1024); y+=12;
  tft.setCursor(4,y); tft.printf("Mode: %d", c_mode);
  String txt = "SYSTEM MONITOR\n";
  txt += "SSID: " + WiFi.SSID() + "\n";
  txt += "Signal: " + String(rssi) + " dBm\n";
  txt += "Ch: " + String(WiFi.channel()) + "\n";
  txt += "ADSB API: "    + String(snap_adsb ? "OK" : "FAIL") + "\n";
  txt += "Weather API: " + String(snap_wx   ? "OK" : "FAIL") + "\n";
  txt += "Last update: " + String(ago) + "s ago\n";
  txt += "Heap: " + String(ESP.getFreeHeap()/1024) + " KB";
  setPreview(txt);
}

void updateMode() {
  if (savePending) return;
  updating = true;
  int c_mode;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_mode = mode;
  xSemaphoreGive(configMutex);
  switch (c_mode) {
    case 1: modeFlight();  break;
    case 2: modeAirport(); break;
    case 3: modeMap();     break;
    case 4: modeWeather(); break;
    case 5: modeClock();   break;
    case 6: modeSystem();  break;
    default: drawText("UNKNOWN MODE " + String(c_mode));
  }
  updating = false;
}
