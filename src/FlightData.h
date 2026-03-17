#ifndef FLIGHT_DATA_H
#define FLIGHT_DATA_H

#include <Arduino.h>
#include <ArduinoJson.h>

extern float inferred_apt_lat;
extern float inferred_apt_lon;
extern String inferred_apt_code;
extern String inferred_apt_name;
extern String inferred_apt_city;
extern String inferred_apt_icao;

bool fetchAirportInference(String flight, int baro_rate, float acLat, float acLon);
void inferAirport(JsonArray ac); // Wrapper for convenience if needed, but we'll use the specific one for memory safety

#endif // FLIGHT_DATA_H
