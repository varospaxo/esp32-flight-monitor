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
#include <AnimatedGIF.h>
#include <LittleFS.h>

static AnimatedGIF gif;
static File gifFile;
static int gifXOffset = 0;
static int gifYOffset = 0;

static void * GIFOpenFile(const char *fname, int32_t *pSize) {
  if (gifFile) gifFile.close();
  gifFile = LittleFS.open(fname, "r");
  if (gifFile) {
    *pSize = gifFile.size();
    return (void *)&gifFile;
  }
  return NULL;
}

static void GIFCloseFile(void *pHandle) {
  File *f = (File *)pHandle;
  if (f && *f) f->close();
}

static int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead = iLen;
  File *f = (File *)pFile->fHandle;
  if (f && *f) {
    if ((pFile->iSize - pFile->iPos) < iLen)
      iBytesRead = pFile->iSize - pFile->iPos;
    if (iBytesRead <= 0) return 0;
    iBytesRead = f->read(pBuf, iBytesRead);
    pFile->iPos += iBytesRead;
    return iBytesRead;
  }
  return 0;
}

static int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = (File *)pFile->fHandle;
  if (f && *f) {
    f->seek(iPosition);
    pFile->iPos = (int32_t)f->position();
    return pFile->iPos;
  }
  return 0;
}

static void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *palette;
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX + gifXOffset > 320)
    iWidth = 320 - (pDraw->iX + gifXOffset);
  y = pDraw->iY + pDraw->y + gifYOffset;
  if (y >= 240 || pDraw->iX + gifXOffset >= 320 || iWidth <= 0)
    return;

  palette = pDraw->pPalette;
  s = pDraw->pPixels;

  tft.setSwapBytes(true);

  if (pDraw->ucHasTransparency) {
    uint8_t ucTransparent = pDraw->ucTransparent;
    int drawX = pDraw->iX + gifXOffset;
    uint16_t usLine[320];
    int startX = -1;
    for (x = 0; x < iWidth; x++) {
      if (s[x] != ucTransparent) {
        usLine[x] = palette[s[x]];
        if (startX == -1) startX = x;
      } else {
        if (startX != -1) {
          tft.pushImage(drawX + startX, y, x - startX, 1, &usLine[startX]);
          startX = -1;
        }
      }
    }
    if (startX != -1) {
      tft.pushImage(drawX + startX, y, iWidth - startX, 1, &usLine[startX]);
    }
  } else {
    uint16_t usLine[320];
    for (x = 0; x < iWidth; x++) {
      usLine[x] = palette[s[x]];
    }
    tft.pushImage(pDraw->iX + gifXOffset, y, iWidth, 1, usLine);
  }
  
  tft.setSwapBytes(false); // Reset to default
}

void modeFlight() {
  if (WiFi.status() != WL_CONNECTED) { drawText("NO WIFI\nCONNECT TO: ESP32-Radar\nCONFIG AT: 192.168.4.1"); return; }
  float c_lat, c_lon; int c_range;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_lat = lat; c_lon = lon; c_range = range_km;
  xSemaphoreGive(configMutex);
  String api = "https://opendata.adsb.fi/api/v3/lat/" + String(c_lat, 4)
             + "/lon/" + String(c_lon, 4)
             + "/dist/" + String(c_range * 0.54f, 1);
  Log.printf("ADSB.fi fetch: %s\n", api.c_str());
  struct AcInfo { 
    String flight; int alt; float dist; float spd; String reg; float hdg; int vspd; String type; String cat; String country; 
    float lat; float lon;
  };
  AcInfo sorted[3]; float dists[3] = {9999,9999,9999}; int count = 0;
  bool snap_fGround, snap_fGlider;
  int totalAcCount = 0;
  {
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(10000); http.begin(client, api);
    http.setUserAgent("Mozilla/5.0");
    int code = http.GET();
    Log.printf("ADSB.fi status: %d\n", code);
    if (code != 200) { drawText("FLIGHT ERR " + String(code)); http.end(); return; }
    adsbOk = true; lastSuccess = millis();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) { 
      Log.printf("FLIGHT PARSE ERR: %s\n", err.c_str());
      drawText("FLIGHT PARSE ERR"); return; 
    }
    JsonArray ac = doc["ac"].as<JsonArray>();
    totalAcCount = ac.size();
    int airCount = 0, groundCount = 0;
    for (JsonObject a : ac) {
      if (a["alt_baro"].as<String>() == "ground") groundCount++;
      else airCount++;
    }
    Log.printf("ADSB flights found: %d (Air: %d, Ground: %d)\n", totalAcCount, airCount, groundCount);
    if (totalAcCount == 0) { drawText("NO AIRCRAFT\nIN RANGE"); return; }
    xSemaphoreTake(configMutex, portMAX_DELAY);
    snap_fGround = filterGround; snap_fGlider = filterGliders;
    xSemaphoreGive(configMutex);
    for (JsonObject a : ac) {
      if (!a["alt_baro"].is<int>()) continue;
      int alt = a["alt_baro"].as<int>();
      if (snap_fGround && alt <= 0) continue;
      String catCode = a["category"] | "";
      if (snap_fGlider && (catCode == "A4" || catCode == "C0")) continue;
      float d    = a["dst"]    | 9999.0f;
      float spd  = a["gs"]     | 0.0f;
      float hdg  = a["track"]  | -1.0f;
      int vspd   = a["baro_rate"] | 0;
      String fl  = a["flight"] | "???"; fl.trim();
      String reg = a["r"]      | "";
      String type = a["t"]     | "";
      String country = a["trc"] | ""; 
      float alat = a["lat"]    | 0.0f;
      float alon = a["lon"]    | 0.0f;
      for (int i = 0; i < 3; i++) {
        if (d < dists[i]) {
          for (int j = 2; j > i; j--) { dists[j] = dists[j-1]; sorted[j] = sorted[j-1]; }
          dists[i] = d; 
          sorted[i] = {fl, alt, d, spd, reg, hdg, vspd, type, catCode, country, alat, alon};
          if (count < 3) count++;
          break;
        }
      }
    }
  } // doc destroyed here
  String airline, route, acType, originName, destName, originCountry, destCountry, originIcao, destIcao, originCity, destCity, originIata, destIata;
  if (sorted[0].flight.length() > 2 && ESP.getFreeHeap() > 60000) {
    WiFiClientSecure client2; client2.setInsecure();
    HTTPClient http2; http2.setTimeout(5000);
    http2.begin(client2, "https://api.adsbdb.com/v0/callsign/" + sorted[0].flight);
    http2.setUserAgent("Mozilla/5.0");
    if (http2.GET() == 200) {
      JsonDocument db;
      DeserializationError err = deserializeJson(db, http2.getStream());
      if (!err) {
        JsonObject fr = db["response"]["flightroute"];
        if (!fr.isNull()) {
          JsonObject al = fr["airline"];
          if (!al.isNull()) airline = al["name"] | "";
          JsonObject orig = fr["origin"];
          JsonObject dest = fr["destination"];
          if (!orig.isNull() && !dest.isNull()) {
            originName = orig["name"] | "";
            originIcao = orig["icao_code"] | "";
            originCountry = orig["country_iso_name"] | "";
            originCity = orig["municipality"] | "";
            originIata = (orig["iata_code"] | orig["iata"]) | "";
            destName = dest["name"] | "";
            destIcao = dest["icao_code"] | "";
            destCountry = dest["country_iso_name"] | "";
            destCity = dest["municipality"] | "";
            destIata = (dest["iata_code"] | dest["iata"]) | "";
            String oCode = originIata.length() ? originIata : originIcao;
            String dCode = destIata.length() ? destIata : destIcao;
            if (oCode.length() && dCode.length()) route = oCode + "-" + dCode;
          }
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
  // Build clean city/name strings for display and preview
  String fullOrig = "";
  if (originName.length()) {
    if (originCity.length() && originName.indexOf(originCity) == -1) fullOrig += originCity + ", ";
    String oCode = originIcao.length() ? originIcao : originIata;
    if (oCode.length()) fullOrig += oCode + " - ";
    fullOrig += originName;
  }
  String fullDest = "";
  if (destName.length()) {
    if (destCity.length() && destName.indexOf(destCity) == -1) fullDest += destCity + ", ";
    String dCode = destIcao.length() ? destIcao : destIata;
    if (dCode.length()) fullDest += dCode + " - ";
    fullDest += destName;
  }
  tftClear();
  tft.fillRect(0, 0, 320, 22, TFT_YELLOW);
  tft.setTextColor(TFT_BLACK, TFT_YELLOW); tft.setTextSize(1);
  tft.setCursor(4, 8); tft.printf("OVERHEAD TRACKER  %dkm", c_range); 
  tft.setCursor(290, 8); tft.print(String(1) + "/" + String(totalAcCount)); 
  int y = 26; 
  int snap_units;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  snap_units = units;
  xSemaphoreGive(configMutex);
  float dAlt = sorted[0].alt, dSpd = sorted[0].spd, dVSpd = sorted[0].vspd, dDist = sorted[0].dist;
  const char* uAlt = "FT", *uSpd = "KT", *uVSpd = "FPM", *uDist = "NM";
  if (snap_units == 1) { // Metric
    dAlt *= 0.3048f; uAlt = "M";
    dSpd *= 1.852f;  uSpd = "KM/H";
    dVSpd *= 0.3048f; uVSpd = "M/M";
    dDist *= 1.852f; uDist = "KM";
  }
  // Row 1: Callsign & Airline
  tft.setTextSize(3);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(4, y); tft.print(sorted[0].flight);
  y += 24;
  tft.setTextSize(2);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setCursor(4, y); tft.print(airline.length() ? airline : "UNKNOWN");
  // Airline Logo
  String icao_al = "";
  for (int i = 0; i < (int)sorted[0].flight.length(); i++) {
    char c = sorted[0].flight.charAt(i);
    if (c >= 'A' && c <= 'Z') icao_al += c;
    if (icao_al.length() == 3) break;
  }
  if (icao_al.length() == 3 && ESP.getFreeHeap() > 60000) {
    WiFiClientSecure clientLogo; clientLogo.setInsecure();
    HTTPClient httpLogo; httpLogo.setTimeout(5000);
    String logoUrl = "https://images.weserv.nl/?url=images.flightradar24.com/assets/airlines/logotypes/" + icao_al + "_logo0.png&output=jpg&w=200&h=200&fit=contain&bg=black";
    httpLogo.begin(clientLogo, logoUrl);
    httpLogo.setUserAgent("Mozilla/5.0");
    int logoCode = httpLogo.GET();
    Log.printf("Logo fetch status: %d\n", logoCode);
    if (logoCode == 200) {
      int lx = 260, ly = 32, lsize = 50;
      String payload = httpLogo.getString();
      if (payload.length() > 0) {
        TJpgDec.setJpgScale(4);
        TJpgDec.drawJpg(lx, ly, (const uint8_t*)payload.c_str(), payload.length());
      }
      payload = "";
    }
    httpLogo.end();
  }
  y += 18;
  y = 86; // Prevent intersection with logo ending at y=84
  tft.drawFastHLine(4, y, 250, TFT_CYAN); y += 4;
  // Row 2: Type & Reg & Route
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(4, y); tft.print("AIRCRAFT TYPE");
  tft.setCursor(110, y); tft.print("REGISTRATION");
  tft.setCursor(200, y); tft.print("ROUTE");
  y += 10;
  String modelStr = acType;
  if (modelStr.length() == 0) modelStr = sorted[0].type;
  if (modelStr.length() == 0) modelStr = categoryName(sorted[0].cat);
  String routeStr = "UNKNOWN";
  String oCode = originIata.length() ? originIata : originIcao;
  String dCode = destIata.length() ? destIata : destIcao;
  if (oCode.length() && dCode.length()) routeStr = oCode + " > " + dCode;
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(4, y); tft.print(modelStr.substring(0, 8));
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(110, y); tft.print(sorted[0].reg.length() ? sorted[0].reg : "???");
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.setCursor(200, y); tft.print(routeStr.substring(0, 10));
  y += 18;
  tft.drawFastHLine(4, y, 312, TFT_CYAN); y += 4;
  // Row 3: Airports
  auto drawAirportLocal = [&](int curY, String name, String country) {
    if (name.length() == 0) return;
    int flagX = 4;
    String cLow = country; cLow.toLowerCase(); cLow.trim();
    if (cLow.length() == 2) {
      String flagUrl = "https://flagcdn.com/w20/" + cLow + ".jpg";
      WiFiClientSecure fcli; fcli.setInsecure();
      HTTPClient fhtp; fhtp.setTimeout(5000); fhtp.begin(fcli, flagUrl);
      int code = fhtp.GET();
      Log.printf("Flag fetch (%s): %d\n", cLow.c_str(), code);
      if (code == 200) {
        String pld = fhtp.getString();
        if (pld.length() > 0) {
          TJpgDec.setJpgScale(1); 
          TJpgDec.drawJpg(flagX, curY - 2, (const uint8_t*)pld.c_str(), pld.length());
        }
      }
      fhtp.end();
    }
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(flagX + 24, curY);
    tft.print(name.substring(0, 42)); // Truncated to prevent overflow
  };
  if (fullOrig.length() || fullDest.length()) {
    drawAirportLocal(y, fullOrig, originCountry); y += 14;
    drawAirportLocal(y, fullDest, destCountry);   y += 14;
  } else {
    y += 28;
  }
  // Row 4: Altitude, Speed, Distance
  tft.drawFastHLine(4, y, 312, TFT_CYAN); y += 4;
  String phase = "CRUISE";
  uint16_t phaseColor = TFT_GREEN;
  if (dAlt < 4000 && dVSpd < -250) { phase = "LANDING"; phaseColor = TFT_RED; }
  else if (dAlt < 4000 && dVSpd > 250) { phase = "TAKEOFF"; phaseColor = TFT_ORANGE; }
  else if (dVSpd > 250) { phase = "CLIMBING"; phaseColor = TFT_ORANGE; }
  else if (dVSpd < -250) { phase = "DESCENT"; phaseColor = TFT_RED; }
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(4, y); tft.print("PHASE");
  tft.setCursor(100, y); tft.printf("ALT (%s)", uAlt);
  tft.setCursor(190, y); tft.printf("SPD (%s)", uSpd);
  tft.setCursor(265, y); tft.printf("DST (%s)", uDist);
  y += 10;
  tft.setTextSize(2);
  tft.setTextColor(phaseColor, TFT_BLACK);
  tft.setCursor(4, y); tft.print(phase);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(100, y); tft.printf("%.0f", dAlt);
  tft.setCursor(190, y); tft.printf("%.0f", dSpd);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(265, y); tft.printf("%.1f", dDist);
  y += 18;
  uint16_t vsColor = dVSpd > 100 ? TFT_GREEN : (dVSpd < -100 ? TFT_RED : TFT_WHITE);
  tft.setTextSize(1);
  tft.setTextColor(vsColor, TFT_BLACK);
  tft.setCursor(4, y); tft.printf("%.0f %s", dVSpd, uVSpd);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  if (sorted[0].hdg >= 0) {
    tft.setCursor(110, y); tft.printf("HDG: %.0f\xC2\xB0", sorted[0].hdg);
  }
  y += 12;
  // Bottom: Secondary Aircraft
  tft.drawFastHLine(4, y, 312, TFT_DARKGREY); y += 4;
  if (count > 1) {
    for (int i = 1; i < count; i++) {
        float sAlt = sorted[i].alt, sDist = sorted[i].dist, sSpd = sorted[i].spd, sHdg = sorted[i].hdg;
        if (snap_units == 1) { sAlt *= 0.3048f; sDist *= 1.852f; sSpd *= 1.852f; }

        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(4, y); 
        tft.print(sorted[i].flight.substring(0,6));

        tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
        tft.setCursor(85, y);
        tft.printf("%.0f", sAlt);

        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setCursor(155, y);
        tft.printf("%.0f", sSpd);

        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.setCursor(205, y);
        tft.printf("%03d", (int)sHdg);

        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(255, y);
        tft.printf("%.1f", sDist);

        y += 18;
        if (y > 230) break;
    }
  }
  String ptxt = "FLIGHT RADAR\n" + sorted[0].flight;
  ptxt += "|" + route; 
  ptxt += "|" + String(1) + "/" + String(totalAcCount); // Index/Total
  ptxt += "|" + String(dAlt, 0) + uAlt;
  ptxt += "|" + String(dVSpd, 0) + uVSpd;
  ptxt += "|" + String(dSpd, 0) + uSpd;
  ptxt += "|" + String(dDist, 1) + uDist; // Dist field
  ptxt += "|" + sorted[0].reg;
  ptxt += "|" + modelStr;
  ptxt += "|" + formatDMS(sorted[0].lat, true) + " " + formatDMS(sorted[0].lon, false);
  ptxt += "|" + (sorted[0].hdg >= 0 ? String(sorted[0].hdg, 0) : "---");
  ptxt += "|" + airline;
  ptxt += "|" + fullOrig + "|" + originCountry;
  ptxt += "|" + fullDest + "|" + destCountry;
  for (int i = 1; i < count; i++) {
    float sAlt = sorted[i].alt, sDist = sorted[i].dist, sSpd = sorted[i].spd, sHdg = sorted[i].hdg;
    if (snap_units == 1) { sAlt *= 0.3048f; sDist *= 1.852f; sSpd *= 1.852f; }
    char sbuf[128];
    snprintf(sbuf, sizeof(sbuf), "SEC|%s|%.0f|%.0f|%03d|%.1f", sorted[i].flight.substring(0,6).c_str(), sAlt, sSpd, (int)sHdg, sDist);
    ptxt += "\n" + String(sbuf);
  }
  setPreview(ptxt);
}
void modeAirport() {
  if (WiFi.status() != WL_CONNECTED) { drawText("NO WIFI\nCONNECT TO: ESP32-Radar\nCONFIG AT: 192.168.4.1"); return; }
  float c_lat, c_lon; int c_range;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_lat = lat; c_lon = lon; c_range = range_km;
  xSemaphoreGive(configMutex);
  String api = "https://opendata.adsb.fi/api/v3/lat/" + String(c_lat,4) + "/lon/" + String(c_lon,4) + "/dist/" + String(c_range * 0.54f, 1);
  Log.printf("ADSB.fi Airport fetch: %s\n", api.c_str());
  struct AcEntry { String flight; int alt; float dist; };
  AcEntry arrivals[5]; int arrCount = 0;
  AcEntry departures[5]; int depCount = 0;
  String infFlight = ""; int infRate = 0; float infLat = 0, infLon = 0;
  {
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(8000); http.begin(client, api);
    http.setUserAgent("Mozilla/5.0");
    int code = http.GET();
    Log.printf("ADSB.fi Airport status: %d\n", code);
    if (code != 200) { drawText("AIRPORT ERR " + String(code)); http.end(); return; }
    adsbOk = true; lastSuccess = millis();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end(); // Close connection early
    if (err) { drawText("PARSE ERR"); return; }
    JsonArray ac = doc["ac"].as<JsonArray>();
    int total = ac.size();
    int airCount = 0, groundCount = 0;
    for (JsonObject a : ac) {
      if (a["alt_baro"].as<String>() == "ground") groundCount++;
      else airCount++;
    }
    Log.printf("ADSB Airport ac.size: %d (Air: %d, Ground: %d)\n", total, airCount, groundCount);
    float best_alt = 99999;
    for (JsonObject a : ac) {
      if (!a["alt_baro"].is<int>()) continue;
      int alt = a["alt_baro"].as<int>();
      if (alt <= 0) continue;
      int baro_rate = a["baro_rate"] | 0;
      if (alt < 10000) {
        String fl = a["flight"] | "???"; fl.trim();
        float d = a["dst"] | 999.0f;
        if (baro_rate < -200) { // Arrival
          if (arrCount < 5) {
            int pos = arrCount;
            for (int i = 0; i < arrCount; i++) if (alt < arrivals[i].alt) { pos = i; break; }
            for (int j = arrCount; j > pos; j--) arrivals[j] = arrivals[j-1];
            arrivals[pos] = {fl, alt, d}; arrCount++;
          }
        } else if (baro_rate > 200) { // Departure
          if (depCount < 5) {
            int pos = depCount;
            for (int i = 0; i < depCount; i++) if (alt < departures[i].alt) { pos = i; break; }
            for (int j = depCount; j > pos; j--) departures[j] = departures[j-1];
            departures[pos] = {fl, alt, d}; depCount++;
          }
        }
      }
      if (alt < best_alt && alt < 5000) {
        best_alt = alt;
        infFlight = a["flight"] | ""; infFlight.trim();
        infLat = a["lat"] | 0.0f;
        infLon = a["lon"] | 0.0f;
        infRate = baro_rate;
      }
    }
  } // doc and http stream are destroyed here
  if (infFlight.length() > 2) {
    fetchAirportInference(infFlight, infRate, infLat, infLon);
  }
  String airport = inferred_apt_code == "---" ? "LOCAL" : inferred_apt_code;
  tftClear(); tftHeader((" " + airport + " ARRIVALS (" + String(c_range) + "km)").c_str(), TFT_ORANGE);
  int y = 28;
  if (inferred_apt_name.length() > 0) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); 
    tft.setCursor(4, y); tft.print(inferred_apt_city); y += 20;
    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(4, y); tft.print(inferred_apt_name); y += 12;
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(4, y); tft.print("ICAO:" + inferred_apt_icao + "  IATA:" + inferred_apt_code + "  ALT:" + String(inferred_apt_alt) + "ft"); y += 16;
  } else { y += 12; }
  if (arrCount == 0 && depCount == 0) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setCursor(4,y); tft.print("No traffic detected");
  } else {
    if (arrCount > 0) {
      tft.setTextSize(1);
      tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setCursor(4, y); tft.print("ARRIVALS"); y += 12;
      for (int i = 0; i < arrCount; i++) {
        tft.setTextColor(TFT_WHITE,    TFT_BLACK); tft.setCursor(4,   y); tft.print(arrivals[i].flight);
        tft.setTextColor(TFT_GREEN,    TFT_BLACK); tft.setCursor(100, y); tft.printf("%d ft", arrivals[i].alt);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK); tft.setCursor(200, y); tft.printf("%.1f nm", arrivals[i].dist);
        y += 14;
      }
      y += 4;
    }
    if (depCount > 0) {
      tft.setTextSize(1);
      tft.setTextColor(TFT_MAGENTA, TFT_BLACK); tft.setCursor(4, y); tft.print("DEPARTURES"); y += 12;
      for (int i = 0; i < depCount; i++) {
        tft.setTextColor(TFT_WHITE,    TFT_BLACK); tft.setCursor(4,   y); tft.print(departures[i].flight);
        tft.setTextColor(TFT_GREEN,    TFT_BLACK); tft.setCursor(100, y); tft.printf("%d ft", departures[i].alt);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK); tft.setCursor(200, y); tft.printf("%.1f nm", departures[i].dist);
        y += 14;
      }
    }
  }
  String coords = String(inferred_apt_lat, 4) + "," + String(inferred_apt_lon, 4);
  String txt = "AIRPORT MONITOR\n" + airport + "\n" + inferred_apt_name + "|" + inferred_apt_city + "|" + inferred_apt_icao + "|" + coords + "|" + String(inferred_apt_alt) + "\n";
  for (int i = 0; i < arrCount; i++)
    txt += "ARR:" + arrivals[i].flight + "|" + String(arrivals[i].alt) + " ft|" + String(arrivals[i].dist,1) + " nm\n";
  for (int i = 0; i < depCount; i++)
    txt += "DEP:" + departures[i].flight + "|" + String(departures[i].alt) + " ft|" + String(departures[i].dist,1) + " nm\n";
  if (arrCount == 0 && depCount == 0) txt += "No traffic detected";
  setPreview(txt);
}
void modeMap() {
  if (WiFi.status() != WL_CONNECTED) { drawText("NO WIFI\nCONNECT TO: ESP32-Radar\nCONFIG AT: 192.168.4.1"); return; }
  float c_lat, c_lon; int c_range;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_lat = lat; c_lon = lon; c_range = range_km;
  xSemaphoreGive(configMutex);
  String api = "https://opendata.adsb.fi/api/v3/lat/" + String(c_lat,4) + "/lon/" + String(c_lon,4) + "/dist/" + String(c_range * 0.54f, 1);
  Log.printf("ADSB.fi Map fetch: %s\n", api.c_str());
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(10000); http.begin(client, api);
  http.setUserAgent("Mozilla/5.0");
  int code = http.GET();
  Log.printf("ADSB.fi Map status: %d\n", code);
    if (code != 200) { drawText("MAP ERR " + String(code)); http.end(); return; }
  adsbOk = true; lastSuccess = millis();
  String bestInfFlight = ""; int bestInfRate = 0; float bestInfLat = 0, bestInfLon = 0;
  String txt = "RADAR MAP\n";
  {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end(); // Close connection
    if (err) { 
      Log.printf("MAP PARSE ERR: %s\n", err.c_str());
      drawText("MAP PARSE ERR"); return; 
    }
    JsonArray ac = doc["ac"].as<JsonArray>();
    int total = ac.size();
    int airCount = 0, groundCount = 0;
    for (JsonObject a : ac) {
      if (a["alt_baro"].as<String>() == "ground") groundCount++;
      else airCount++;
    }
    Log.printf("MAP ac.size: %d (Air: %d, Ground: %d)\n", total, airCount, groundCount);
    tftClear();
    const int cx = 160, cy = 130, maxR = 100;
    tft.drawCircle(cx, cy, maxR,     TFT_DARKCYAN);
    tft.drawCircle(cx, cy, maxR / 2, TFT_DARKCYAN);
    tft.drawCircle(cx, cy, 3,        TFT_WHITE);
    tft.drawFastHLine(cx - maxR, cy, maxR * 2, TFT_DARKCYAN);
    tft.drawFastVLine(cx, cy - maxR, maxR * 2, TFT_DARKCYAN);
    tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setTextSize(2);
    tft.setCursor(4, 2); tft.printf("RADAR  %dkm", c_range);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(cx + maxR + 4, cy - 4); tft.printf("%dkm", c_range);
    tft.setCursor(cx + maxR/2 - 8, cy + 4); tft.printf("%d", c_range/2);
    float rangeNm = c_range * 0.54f;
    int plotCount = 0;
    float best_alt = 99999;
    for (JsonObject a : ac) {
      if (plotCount >= 15) break;
      if (!a["alt_baro"].is<int>()) continue;
      int alt = a["alt_baro"].as<int>();
      if (alt <= 0) continue;
      float d     = a["dst"]    | 9999.0f;
      float acLat = a["lat"]    | 0.0f;
      float acLon = a["lon"]    | 0.0f;
      String fl   = a["flight"] | ""; fl.trim();
      if (fl.length() == 0) {
        fl = a["r"].as<String>();
        if (fl == "undefined" || fl.length() == 0) fl = a["hex"].as<String>();
        if (fl == "undefined" || fl.length() == 0) fl = "?";
      }
      String type = a["t"]      | "";
      if (d > rangeNm || acLat == 0) continue;
      if (alt < best_alt && alt < 5000) {
        best_alt = alt;
        bestInfFlight = fl;
        bestInfLat = acLat;
        bestInfLon = acLon;
        bestInfRate = a["baro_rate"] | 0;
      }
      float dx = (acLon - c_lon) * cos(radians(c_lat));
      float dy = (acLat - c_lat);
      float maxDeg = c_range / 111.0f;
      if (maxDeg == 0) continue;
      int px = constrain(cx + (int)((dx / maxDeg) * maxR), 10, 310);
      int py = constrain(cy - (int)((dy / maxDeg) * maxR), 20, 230);
      float track = a["track"] | -1.0f;
      if (track >= 0) {
        float trad = (track - 90) * PI / 180.0;
        int x2 = px + cos(trad) * 10;
        int y2 = py + sin(trad) * 10;
        tft.drawLine(px, py, x2, y2, TFT_CYAN);
        tft.fillCircle(px, py, 2, TFT_CYAN);
      } else {
        tft.fillCircle(px, py, 3, TFT_CYAN);
      }
      tft.setTextSize(1);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.setCursor(px + 6, py - 8); tft.print(fl);
      if (type.length()) { 
        tft.setTextSize(1); 
        tft.setCursor(px + 6, py + 8); 
        tft.print(type); 
      }
      Log.printf("Plotted AC: %s at %d,%d (dist %.1fnm)\n", fl.c_str(), px, py, d);
      float bearingRad = atan2(dx, dy);
      int bearing = ((int)(degrees(bearingRad) + 360)) % 360;
      txt += fl + " (" + type + ") " + String(d,1) + "nm B" + String(bearing) + " T" + String((int)track) + "\n";
      plotCount++;
    }
  } // doc destroyed here
  if (bestInfFlight.length() > 2) {
    fetchAirportInference(bestInfFlight, bestInfRate, bestInfLat, bestInfLon);
  }
  if (inferred_apt_lat != 0 && inferred_apt_code != "---") {
    float apt_d_km = haversine(c_lat, c_lon, inferred_apt_lat, inferred_apt_lon);
    float apt_d_nm = apt_d_km / 1.852f;
    if (apt_d_km <= (float)c_range) {
      float apt_dx = (inferred_apt_lon - c_lon) * cos(radians(c_lat));
      float apt_dy = (inferred_apt_lat - c_lat);
      float maxDeg = (float)c_range / 111.0f;
      if (maxDeg > 0) {
        const int cx = 160, cy = 130, maxR = 100;
        int apx = cx + (int)((apt_dx / maxDeg) * maxR);
        int apy = cy - (int)((apt_dy / maxDeg) * maxR);
        // Constrain to slightly within radar circles for visibility
        apx = constrain(apx, cx - maxR, cx + maxR);
        apy = constrain(apy, cy - maxR, cy + maxR);
        tft.fillCircle(apx, apy, 4, TFT_RED);
        tft.setTextSize(2);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(apx + 6, apy - 8); tft.print(inferred_apt_code);
        float apt_bearingRad = atan2(apt_dx, apt_dy);
        int apt_bearing = ((int)(degrees(apt_bearingRad) + 360)) % 360;
        txt += "[APT] " + inferred_apt_code + " " + String(apt_d_nm, 1) + "nm B" + String(apt_bearing) + " T0\n";
        Log.printf("Plotted Airport: %s at %d,%d (dist %.1fkm)\n", inferred_apt_code.c_str(), apx, apy, apt_d_km);
      }
    } else {
      Log.printf("Airport %s too far: %.1fkm (range %dkm)\n", inferred_apt_code.c_str(), apt_d_km, c_range);
    }
  }
  if (txt == "RADAR MAP\n") txt += "No aircraft in range";
  Log.println("MAP Preview: " + txt);
  setPreview(txt);
}
void modeWeather() {
  if (WiFi.status() != WL_CONNECTED) { drawText("NO WIFI\nCONNECT TO: ESP32-Radar\nCONFIG AT: 192.168.4.1"); return; }
  float c_lat, c_lon;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_lat = lat; c_lon = lon;
  xSemaphoreGive(configMutex);
  String url = "https://api.open-meteo.com/v1/forecast?latitude="  + String(c_lat,4) + "&longitude=" + String(c_lon,4) + "&current=temperature_2m,relative_humidity_2m,uv_index,precipitation,wind_speed_10m,wind_direction_10m,visibility";
  Log.printf("Weather fetch: %s\n", url.c_str());
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(8000); http.begin(client, url);
  float temp = 0, rain = 0; int humidity = 0, uv = 0; float wind = 0, windDir = 0; int visibility = 0;
  int wCode = http.GET();
  Log.printf("Weather status: %d\n", wCode);
  if (wCode == 200) {
    String payload = http.getString();
    Log.println("Weather JSON: " + payload);
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      JsonObject c = doc["current"].as<JsonObject>();
      temp     = c["temperature_2m"].as<float>();
      humidity = c["relative_humidity_2m"].as<int>();
      uv       = c["uv_index"].as<float>();
      rain     = c["precipitation"].as<float>();
      wind     = c["wind_speed_10m"].as<float>();
      windDir  = c["wind_direction_10m"].as<float>();
      visibility = c["visibility"].as<float>();
      weatherOk = true; lastSuccess = millis();
    }
  }
  http.end();
  int aqi = -1;
  HTTPClient http2; http2.setTimeout(5000);
  String aqUrl = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude="  + String(c_lat,4) + "&longitude=" + String(c_lon,4) + "&current=us_aqi";
  Log.printf("AQI fetch: %s\n", aqUrl.c_str());
  WiFiClientSecure client2; client2.setInsecure();
  http2.begin(client2, aqUrl);
  int aqCode = http2.GET();
  Log.printf("AQI status: %d\n", aqCode);
  if (aqCode == 200) {
    String payload = http2.getString();
    Log.println("AQI JSON: " + payload);
    JsonDocument doc2;
    DeserializationError err2 = deserializeJson(doc2, payload);
    if (!err2) {
      if (doc2["current"]["us_aqi"].isNull()) aqi = -1;
      else aqi = doc2["current"]["us_aqi"].as<int>();
    }
  }
  http2.end();
  
  tftClear(); tftHeader(" WEATHER", 0x07E0);

  // Each cell: 160px wide, 44px tall, label at top, value below
  // Layout: 2 cols x 4 rows starting at y=26
  // Col 0 x=0, Col 1 x=160
  // Rows y = 26, 70, 114, 158
  struct WCell { String label; uint16_t lColor; String value; uint16_t vColor; } cells[8];
  cells[0] = {"TEMPERATURE", TFT_YELLOW,  String(temp, 1) + " C",                   TFT_WHITE};
  cells[1] = {"HUMIDITY",    TFT_CYAN,    String(humidity) + "%",                    TFT_WHITE};
  cells[2] = {"WIND SPEED",  TFT_ORANGE,  String(wind, 1) + " kph",                 TFT_WHITE};
  cells[3] = {"WIND DIR",    TFT_ORANGE,  String((int)windDir) + " deg",             TFT_WHITE};
  cells[4] = {"UV INDEX",    TFT_RED,     String(uv),                                TFT_WHITE};
  cells[5] = {"PRECIP",      TFT_CYAN,    String(rain, 1) + " mm",                  TFT_WHITE};
  cells[6] = {"VISIBILITY",  TFT_MAGENTA, String(visibility / 1000) + " km",        TFT_WHITE};
  uint16_t aqiColor = aqi > 150 ? TFT_RED : aqi > 100 ? TFT_ORANGE : aqi > 50 ? TFT_YELLOW : TFT_GREEN;
  cells[7] = {"AQI",         aqiColor,    aqi >= 0 ? String(aqi) + " " + aqiLabel(aqi) : "N/A", aqiColor};

  // 4 rows × 2 cols fills 240-26 = 214px → 53px per row with natural top/bottom padding
  const int cellW = 160, cellH = 53;
  for (int i = 0; i < 8; i++) {
    int cx = (i % 2) * cellW;
    int cy = 26 + (i / 2) * cellH;
    if (i > 0) {
      if (i % 2 == 0) tft.drawFastHLine(0, cy, 320, TFT_DARKGREY);
      if (i % 2 == 1) tft.drawFastVLine(160, cy, cellH, TFT_DARKGREY);
    }
    tft.setTextSize(1);
    tft.setTextColor(cells[i].lColor, TFT_BLACK);
    tft.setCursor(cx + 4, cy + 6);
    tft.print(cells[i].label.c_str());
    tft.setTextSize(2);
    tft.setTextColor(cells[i].vColor, TFT_BLACK);
    tft.setCursor(cx + 4, cy + 18);
    tft.print(cells[i].value);
  }
  
  String txt = "WEATHER\n";
  txt += "Temperature   " + String(temp,1) + " C\n";
  txt += "Humidity      " + String(humidity) + "%\n";
  txt += "Precipitation " + String(rain, 1) + " mm\n";
  txt += "UV Index      " + String(uv) + "\n";
  txt += "Wind Speed    " + String(wind,1) + " kph\n";
  txt += "Wind Dir      " + String((int)windDir) + "°\n";
  txt += "Visibility    " + String(visibility / 1000) + " km\n";
  if (aqi >= 0) txt += "AQI           " + String(aqi) + " (" + aqiLabel(aqi) + ")";
  setPreview(txt);
}
static String lastDateStr = "";
static String lastTzStr = "";

void modeClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { drawText("NTP SYNC..."); return; }
  String c_timezone, c_tzAbbr;
  long c_offset;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_timezone = timezone;
  c_offset = tzOffset;
  c_tzAbbr = tzAbbr;
  xSemaphoreGive(configMutex);

  char timeBuf[9], dateBuf[20];
  strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S",    &timeinfo);
  strftime(dateBuf, sizeof(dateBuf), "%a, %d %b %Y",&timeinfo);

  Log.printf("Current time: %s\n", timeBuf);

  int h = abs(c_offset) / 3600;
  int m = (abs(c_offset) % 3600) / 60;
  char gmtStr[16];
  snprintf(gmtStr, sizeof(gmtStr), "GMT%s%02d:%02d", (c_offset >= 0 ? "+" : "-"), h, m);
  String fullTz = String(gmtStr);
  if (c_tzAbbr.length() > 0) {
      String upperAbbr = c_tzAbbr;
      upperAbbr.toUpperCase();
      if (!upperAbbr.startsWith("GMT+") && !upperAbbr.startsWith("GMT-") && 
          !upperAbbr.startsWith("UTC+") && !upperAbbr.startsWith("UTC-")) {
          fullTz += " (" + c_tzAbbr + ")";
      }
  }

  bool fullRedraw = (lastDrawnMode != 5) || (lastDateStr != String(dateBuf)) || (lastTzStr != (fullTz + c_timezone));

  if (fullRedraw) {
    tftClear();
    tft.fillRect(0, 0, 320, 22, TFT_CYAN);
    tft.setTextColor(TFT_BLACK, TFT_CYAN); tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("CLOCK", 4, 7);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); 
    tft.setTextSize(3);
    tft.drawString(dateBuf, 160, 140);

    tft.setTextColor(TFT_WHITE, TFT_BLACK); 
    tft.setTextSize(2);
    tft.drawString(fullTz, 160, 180);

    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK); 
    tft.setTextSize(2);
    tft.drawString(c_timezone, 160, 210);

    lastDrawnMode = 5;
    lastDateStr = String(dateBuf);
    lastTzStr = fullTz + c_timezone;
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK); 
  tft.setTextSize(6);
  tft.drawString(timeBuf, 160, 80);

  tft.setTextDatum(TL_DATUM);
  setPreview("CLOCK\n" + String(timeBuf) + "\n" + String(dateBuf) + "\n" + fullTz + "\n" + c_timezone);
}
void modeSystem() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure c; c.setInsecure();
    HTTPClient h; h.setTimeout(4000);
    h.begin(c, "https://opendata.adsb.fi/api/v3/lat/0/lon/0/dist/1");
    int aCode = h.GET();
    h.end();
    adsbOk = (aCode == 200);

    h.begin(c, "https://api.open-meteo.com/v1/forecast?latitude=0&longitude=0&current=temperature_2m");
    int wCode = h.GET();
    h.end();
    weatherOk = (wCode == 200);

    if (adsbOk || weatherOk) lastSuccess = millis();
  }

  tftClear(); tftHeader(" SYSTEM MONITOR", TFT_BLUE);
  int y = 28; tft.setTextSize(2);
  int rssi = WiFi.RSSI();
  int bars = rssi > -50 ? 4 : rssi > -60 ? 3 : rssi > -70 ? 2 : rssi > -80 ? 1 : 0;
  unsigned long ago = (millis() - lastSuccess) / 1000;
  int c_mode;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_mode = mode;
  xSemaphoreGive(configMutex);
  bool snap_adsb = adsbOk, snap_wx = weatherOk;
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setCursor(4,y); tft.print("-- WiFi --"); y+=20;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4,y); tft.printf("SSID: %.14s", WiFi.SSID().c_str()); y+=20;
  tft.setCursor(4,y); tft.printf("Sig: %d dBm", rssi);
  for (int i = 0; i < 4; i++) {
    tft.setTextColor(i < bars ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(200 + i*8, y); tft.print("|");
  }
  y+=20;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4,y); tft.printf("IP: %s", WiFi.localIP().toString().c_str()); y+=24;
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setCursor(4,y); tft.print("-- API --"); y+=20;
  tft.setCursor(4,y); tft.setTextColor(snap_adsb ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.printf("ADSB: %s ", snap_adsb ? "OK" : "FAIL");
  tft.setTextColor(snap_wx ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.printf("WX: %s", snap_wx ? "OK" : "FAIL"); y+=20;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4,y); tft.printf("Last : %lus ago", ago); y+=24;
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setCursor(4,y); tft.print("-- Sys --"); y+=20;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4,y); tft.printf("Heap: %d KB", ESP.getFreeHeap()/1024); y+=20;
  tft.setCursor(4,y); tft.printf("Mode: %d", c_mode);
  String txt = "SYSTEM MONITOR\n";
  txt += "SSID: " + WiFi.SSID() + "\n";
  txt += "Signal: " + String(rssi) + " dBm\n";
  txt += "Ch: " + String(WiFi.channel()) + "\n";
  txt += "IP: " + WiFi.localIP().toString() + "\n";
  txt += "ADSB API: "    + String(snap_adsb ? "OK" : "FAIL") + "\n";
  txt += "Weather API: " + String(snap_wx   ? "OK" : "FAIL") + "\n";
  txt += "Last update: " + String(ago) + "s ago\n";
  txt += "Heap: " + String(ESP.getFreeHeap()/1024) + " KB";
  setPreview(txt);
}

// Coordinates for Settings buttons (must match modeSettings draw positions)
#define SETT_AUTOCYCLE_X  4
#define SETT_AUTOCYCLE_Y  26
#define SETT_AUTOCYCLE_W  150
#define SETT_AUTOCYCLE_H  30
#define SETT_BTN_W        96
#define SETT_BTN_H        34
#define SETT_COL0_X       4
#define SETT_COL1_X       112
#define SETT_COL2_X       220
#define SETT_ROW0_Y       74
#define SETT_ROW1_Y       114
#define SETT_ROW2_Y       154

int handleSettingsTouch(uint16_t tx, uint16_t ty) {
  // x-axis is mirrored on this touch controller
  uint16_t x = 320 - tx;
  uint16_t y = ty;

  Log.printf("Settings touch raw=(%d,%d) cal=(%d,%d)\n", tx, ty, x, y);

  if (x >= SETT_AUTOCYCLE_X && x < SETT_AUTOCYCLE_X + SETT_AUTOCYCLE_W &&
      y >= SETT_AUTOCYCLE_Y && y < SETT_AUTOCYCLE_Y + SETT_AUTOCYCLE_H) {
    return 1;
  }

  int colX[3] = {SETT_COL0_X, SETT_COL1_X, SETT_COL2_X};
  int rowY[3] = {SETT_ROW0_Y, SETT_ROW1_Y, SETT_ROW2_Y};

  for (int i = 0; i < NUM_CYCLE_MODES; i++) {
    int m = CYCLE_MODE_LIST[i];
    int col = i % 3;
    int row = i / 3;
    if (x >= colX[col] && x < colX[col] + SETT_BTN_W &&
        y >= rowY[row] && y < rowY[row] + SETT_BTN_H) {
      Log.printf("Settings: toggling mode %d\n", m);
      return 100 + m;
    }
  }

  return 0;
}

void modeSettings() {
  tftClear();
  tftHeader(" SETTINGS", TFT_MAGENTA);

  bool c_autoCycle;
  String c_cycleModes;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_autoCycle = autoCycle;
  c_cycleModes = cycleModes;
  xSemaphoreGive(configMutex);

  // Auto-cycle wide button
  drawButton(SETT_AUTOCYCLE_X, SETT_AUTOCYCLE_Y, SETT_AUTOCYCLE_W, SETT_AUTOCYCLE_H,
             c_autoCycle, c_autoCycle ? "AUTO: ON" : "AUTO: OFF");
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(162, 36);
  tft.printf("%d min interval", max(1, cycleMins));

  // "Modes:" label
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(4, 62);
  tft.print("Cycle modes:");

  // 3-column mode buttons, coordinates match the #defines above
  int colX[3] = {SETT_COL0_X, SETT_COL1_X, SETT_COL2_X};
  int rowY[3] = {SETT_ROW0_Y, SETT_ROW1_Y, SETT_ROW2_Y};
  const char* modeNames[NUM_CYCLE_MODES] = {"FLIGHT", "AIRPORT", "MAP", "WEATHER", "CLOCK", "SYSTEM", "GIF", "TEXT"};

  for (int i = 0; i < NUM_CYCLE_MODES; i++) {
    int m = CYCLE_MODE_LIST[i];
    bool on = cycleModeEnabled(c_cycleModes, m);
    int col = i % 3;
    int row = i / 3;
    drawButton(colX[col], rowY[row], SETT_BTN_W, SETT_BTN_H, on, modeNames[i]);
  }

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(4, 196);
  tft.print("Tap to toggle  |  Swipe to exit");

  String txt = "SETTINGS\nAuto-Cycle: " + String(c_autoCycle ? "ON" : "OFF") + "\n";
  txt += "Enabled Modes: " + c_cycleModes;
  setPreview(txt);
}

int handleNetworkTouch(uint16_t tx, uint16_t ty) {
  // Network info is read-only, no touch handling needed
  return 0;
}

void modeNetworkInfo() {
  tftClear();
  tftHeader(" NETWORK INFO", TFT_GREEN);
  
  int y = 30;
  
  // WiFi Status
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(4, y);
  tft.print("WiFi:");
  y += 22;
  
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(4, y);
    tft.print("Network Name:");
    y += 12;
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(4, y);
    String ssidStr = WiFi.SSID();
    if (ssidStr.length() > 16) ssidStr = ssidStr.substring(0, 16) + "...";
    tft.print(ssidStr);
    y += 24;
    
    // Signal strength
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(4, y);
    int rssi = WiFi.RSSI();
    tft.printf("Signal: %d dBm", rssi);
    y += 14;
    
    // Draw signal bars
    int bars = rssi > -50 ? 4 : rssi > -60 ? 3 : rssi > -70 ? 2 : rssi > -80 ? 1 : 0;
    for (int i = 0; i < 4; i++) {
      tft.setTextColor(i < bars ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
      tft.setCursor(200 + i * 8, y - 4);
      tft.print("|");
    }
    y += 14;
    
    y += 8;
    
    // LAN IP
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(4, y);
    tft.print("IP Address:");
    y += 22;
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(4, y);
    tft.print("LAN:");
    y += 12;
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(4, y);
    tft.print(WiFi.localIP().toString());
    y += 22;
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(4, y);
    tft.printf("GW: %s", WiFi.gatewayIP().toString().c_str());
    
    String txt = "NETWORK INFO\n";
    txt += "WiFi Network: " + WiFi.SSID() + "\n";
    txt += "Signal: " + String(rssi) + " dBm\n";
    txt += "LAN IP: " + WiFi.localIP().toString() + "\n";
    txt += "Gateway: " + WiFi.gatewayIP().toString();
    setPreview(txt);
  } else {
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(4, y);
    tft.print("NOT CONNECTED");
    y += 30;
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(4, y);
    tft.print("AP Mode Available:");
    y += 14;
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(4, y);
    tft.print("SSID: ESP32-Radar");
    y += 12;
    tft.setCursor(4, y);
    tft.print("IP: 192.168.4.1");
    
    setPreview("NETWORK INFO\nNOT CONNECTED\nAP: ESP32-Radar\nIP: 192.168.4.1");
  }
}

// ─── Custom Text Mode ──────────────────────────────────────────────────────
static float marqueeX = 0;
static String lastMarqueeText = "";
static int lastMarqueeDir = -1;
static int lastTextStyle = -1;
static bool marqueeNeedsReset = false;

// Word-wraps text to fit within the TFT width at the given text size, then
// draws it centered (horizontally & vertically) in the area below the header.
static void drawWrappedCenteredText(const String& text, int textSize, uint16_t color) {
  tft.setTextSize(textSize);
  tft.setTextColor(color, TFT_BLACK);
  const int maxWidth = 300;
  const int maxLines = 12;
  String lines[maxLines];
  int lineCount = 0;
  String cur = "";
  int start = 0;
  int len = text.length();
  for (int i = 0; i <= len; i++) {
    if (i == len || text.charAt(i) == ' ') {
      String word = text.substring(start, i);
      start = i + 1;
      if (word.length() == 0) continue;
      String test = cur.length() ? cur + " " + word : word;
      if (tft.textWidth(test) > maxWidth && cur.length() > 0) {
        if (lineCount < maxLines) lines[lineCount++] = cur;
        cur = word;
      } else {
        cur = test;
      }
    }
  }
  if (cur.length() > 0 && lineCount < maxLines) lines[lineCount++] = cur;
  if (lineCount == 0) { lines[0] = text; lineCount = 1; }

  int lineHeight = textSize * 8 + 6;
  int totalHeight = lineCount * lineHeight;
  int areaTop = 24, areaBottom = 236;
  int y = areaTop + max(0, ((areaBottom - areaTop) - totalHeight) / 2);

  tft.setTextDatum(MC_DATUM);
  for (int i = 0; i < lineCount; i++) {
    tft.drawString(lines[i], 160, y + lineHeight / 2);
    y += lineHeight;
  }
  tft.setTextDatum(TL_DATUM);
}

void modeCustomText() {
  String c_text; int c_style, c_dir;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_text = customText; c_style = customTextStyle; c_dir = customTextDirection;
  xSemaphoreGive(configMutex);

  if (c_text.length() == 0) c_text = "No text set.\nConfigure via Web Dashboard.";

  if (c_style == 0) {
    // Static: draw once, no need to redraw every refresh unless the text/style changed
    if (lastDrawnMode != 10 || lastMarqueeText != c_text || lastTextStyle != c_style) {
      tftClear();
      tftHeader(" CUSTOM TEXT", TFT_MAGENTA);
      drawWrappedCenteredText(c_text, 3, TFT_WHITE);
      lastDrawnMode = 10;
      lastMarqueeText = c_text;
      lastTextStyle = c_style;
    }
  } else {
    // Marquee: scroll continuously, resetting position whenever text/direction changes
    bool changed = (lastDrawnMode != 10) || (c_text != lastMarqueeText) || (c_dir != lastMarqueeDir) || (c_style != lastTextStyle);
    if (lastDrawnMode != 10) {
      tftClear();
      tftHeader(" CUSTOM TEXT", TFT_MAGENTA);
    }
    if (changed) {
      tft.setTextSize(5);
      int textW = tft.textWidth(c_text);
      marqueeX = (c_dir == 0) ? 320 : -textW;
      lastMarqueeText = c_text;
      lastMarqueeDir = c_dir;
      lastTextStyle = c_style;
      marqueeNeedsReset = false;
    }
    lastDrawnMode = 10;

    if (marqueeNeedsReset) {
      tft.fillScreen(TFT_BLACK); // full reset once the text has scrolled fully off-screen (avoid tftClear(), which would clear lastDrawnMode too)
      tftHeader(" CUSTOM TEXT", TFT_MAGENTA);
      marqueeNeedsReset = false;
    }

    const int y = 92, rowH = 44;
    tft.setTextSize(5);
    int textW = tft.textWidth(c_text);
    tft.fillRect(0, y - 2, 320, rowH, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextWrap(false); // text runs wider than the screen; wrapping to the next line caused ghosting
    tft.setCursor((int)marqueeX, y);
    tft.print(c_text);

    const float speed = 4.0f;
    if (c_dir == 0) { // flow right-to-left
      marqueeX -= speed;
      if (marqueeX < -textW) { marqueeX = 320; marqueeNeedsReset = true; }
    } else {          // flow left-to-right
      marqueeX += speed;
      if (marqueeX > 320) { marqueeX = -textW; marqueeNeedsReset = true; }
    }
  }

  setPreview("CUSTOM TEXT\n" + c_text + "\n" + String(c_style) + "\n" + String(c_dir));
}

void closeGIF() {
  if (gifFile) {
    gif.close();
    gifFile.close();
  }
  if (lastDrawnMode == 7) {
    lastDrawnMode = -1;
  }
}

void modeGIF() {
  if (gifUploadPending) {
    closeGIF();
    setPreview("GIF PLAYER\nUploading...");
    return;
  }

  if (!LittleFS.exists("/custom.gif")) {
    if (lastDrawnMode != 7) {
      tftClear();
      tftHeader(" GIF PLAYER", TFT_MAGENTA);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.setTextSize(2);
      tft.drawString("NO GIF UPLOADED", 160, 100);
      tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft.setTextSize(1);
      tft.drawString("Upload a GIF (<=100KB)", 160, 140);
      tft.drawString("via Web Dashboard Settings", 160, 160);
      tft.setTextDatum(TL_DATUM);
      lastDrawnMode = 7;
    }
    setPreview("GIF PLAYER\nNO GIF UPLOADED\nUpload via Web Dashboard");
    return;
  }

  if (lastDrawnMode != 7) {
    tftClear();
    gif.begin(GIF_PALETTE_RGB565_LE);
    if (gif.open("/custom.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
      gifXOffset = max(0, (320 - gif.getCanvasWidth()) / 2);
      gifYOffset = max(0, (240 - gif.getCanvasHeight()) / 2);
      lastDrawnMode = 7;
    } else {
      drawText("GIF OPEN ERR");
      lastDrawnMode = -1;
      return;
    }
  }

  int delayMs = 100;
  int res = gif.playFrame(true, &delayMs);
  if (res == 0) {
    gif.reset();
  }
  gifDelayMs = delayMs;

  setPreview("GIF PLAYER\nPlaying /custom.gif\nResolution: " + String(gif.getCanvasWidth()) + "x" + String(gif.getCanvasHeight()));
}
void updateMode() {
  if (savePending) { Log.println("updateMode: skip, save pending"); return; }
  if (mode != 7) {
    Log.printf("updateMode: starting mode %d\n", mode);
    closeGIF();
  }
  updating = true;
  int c_mode;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_mode = mode;
  xSemaphoreGive(configMutex);
  switch (c_mode) {
    case 1: modeFlight();       break;
    case 2: modeAirport();      break;
    case 3: modeMap();          break;
    case 4: modeWeather();      break;
    case 5: modeClock();        break;
    case 6: modeSystem();       break;
    case 7: modeGIF();          break;
    case 8: modeSettings();     break;
    case 9: modeNetworkInfo();  break;
    case 10: modeCustomText();  break;
    default: Log.printf("updateMode: unknown mode %d\n", c_mode);
  }
  updating = false;
  if (c_mode != 7) Log.println("updateMode: finished");
}
