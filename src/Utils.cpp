#include "Utils.h"
#include "Globals.h"
#include <math.h>

float haversine(float lat1, float lon1, float lat2, float lon2) {
  float dLat = radians(lat2 - lat1), dLon = radians(lon2 - lon1);
  float a = sin(dLat/2)*sin(dLat/2) +
            cos(radians(lat1))*cos(radians(lat2))*sin(dLon/2)*sin(dLon/2);
  return 6371.0f * 2.0f * atan2(sqrt(a), sqrt(1-a));
}

String aqiLabel(int aqi) {
  if (aqi <= 50)  return "Good";
  if (aqi <= 100) return "Moderate";
  if (aqi <= 150) return "Unhealthy*";
  if (aqi <= 200) return "Unhealthy";
  if (aqi <= 300) return "Very Bad";
  return "Hazardous";
}

const char* headingArrow(float track) {
  if (track < 0) return "?";
  int idx = ((int)(track + 22.5f) % 360) / 45;
  const char* arrows[] = {"N^","NE/","E>","SE\\","Sv","SW/","W<","NW\\"};
  return arrows[idx];
}

void setPreview(const String& s) {
  xSemaphoreTake(previewMutex, portMAX_DELAY);
  preview = s;
  xSemaphoreGive(previewMutex);
}

String formatDMS(float val, bool isLat) {
  char dir = isLat ? (val >= 0 ? 'N' : 'S') : (val >= 0 ? 'E' : 'W');
  val = abs(val);
  int d = (int)val;
  int m = (int)((val - d) * 60);
  int s = (int)((val - d - m/60.0) * 3600);
  char buf[32];
  snprintf(buf, sizeof(buf), "%d\xC2\xB0%d'%d\"%c", d, m, s, dir);
  return String(buf);
}

String categoryName(const String& cat) {
  if (cat == "A1") return "Light";
  if (cat == "A2") return "Small";
  if (cat == "A3") return "Large";
  if (cat == "A4") return "High Vortex";
  if (cat == "A5") return "Heavy";
  if (cat == "A6") return "Highly Maneuverable";
  if (cat == "A7") return "Rotorcraft";
  if (cat == "B1") return "Glider";
  if (cat == "B2") return "Lighter-than-air";
  if (cat == "B3") return "Parachutist";
  if (cat == "B4") return "Ultralight";
  if (cat == "B6") return "UAV";
  if (cat == "B7") return "Space Vehicle";
  if (cat == "C1") return "Surface Vehicle";
  if (cat == "C2") return "Service Vehicle";
  return "";
}
