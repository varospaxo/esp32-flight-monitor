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
