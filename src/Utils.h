#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

float haversine(float lat1, float lon1, float lat2, float lon2);
String aqiLabel(int aqi);
const char* headingArrow(float track);
void setPreview(const String& s);

#endif // UTILS_H
