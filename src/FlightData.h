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

void inferAirport(JsonArray ac);

#endif // FLIGHT_DATA_H
