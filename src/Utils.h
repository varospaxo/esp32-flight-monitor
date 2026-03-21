#ifndef UTILS_H
#define UTILS_H
#include <Arduino.h>
float haversine(float lat1, float lon1, float lat2, float lon2);
String aqiLabel(int aqi);
const char* headingArrow(float track);
String formatDMS(float val, bool isLat);
String categoryName(const String& cat);
void setPreview(const String& s);
#endif // UTILS_H
