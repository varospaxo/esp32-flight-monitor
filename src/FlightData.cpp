#include "FlightData.h"
#include "Globals.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "Utils.h"
#include "Config.h"

float inferred_apt_lat = 0;
float inferred_apt_lon = 0;
String inferred_apt_code = "---";
String inferred_apt_name = "";
String inferred_apt_city = "";
String inferred_apt_icao = "";

void inferAirport(JsonArray ac) {
  if (inferred_apt_code != "---") {
    if (millis() - lastInference < 60000) return; // Wait 1 minute between refreshes
  }

  float best_alt = 99999;
  String best_flight = "";
  float best_lat = 0, best_lon = 0;
  int best_rate = 0;

  for (JsonObject a : ac) {
    if (!a["alt_baro"].is<int>()) continue;
    int alt = a["alt_baro"].as<int>();
    if (alt <= 0 || alt > 12000) continue; // Ignore high altitude or ground
    
    float acLat = a["lat"] | 0.0f;
    float acLon = a["lon"] | 0.0f;
    if (acLat == 0) continue;

    int rate = a["baro_rate"] | 0;
    
    // Priority: Lower is better, but descending/climbing is best
    // We want to find the flight most likely to be interacting with a local airport.
    float score = alt; 
    if (abs(rate) > 300) score *= 0.5f; // Prioritize those with clear vertical movement

    if (score < best_alt) {
      best_alt = score; // Using alt as a base for score
      best_flight = a["flight"] | "";
      best_lat = acLat;
      best_lon = acLon;
      best_rate = rate;
    }
  }

  best_flight.trim();
  if (best_lat != 0 && best_flight.length() > 2) {
    fetchAirportInference(best_flight, best_rate, best_lat, best_lon);
  }
}

bool fetchAirportInference(String flight, int baro_rate, float acLat, float acLon) {
  if (flight.length() < 3) return false;
  if (ESP.getFreeHeap() < 60000) {
    Log.printf("fetchAirportInference: skip, low heap %u\n", ESP.getFreeHeap());
    return false;
  }

  Log.printf("Inferring airport from flight: %s\n", flight.c_str());
  
  // Clear previous state
  inferred_apt_code = "---";
  inferred_apt_name = "";
  inferred_apt_city = "";
  inferred_apt_icao = "";
  inferred_apt_lat = 0;
  inferred_apt_lon = 0;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(5000);
  String url = "https://api.adsbdb.com/v0/callsign/" + flight;
  http.begin(client, url);
  http.setUserAgent("Mozilla/5.0");
  int code = http.GET();
  Log.printf("ADS-B DB Status: %d\n", code);
  
  bool success = false;
  if (code == 200) {
    JsonDocument db;
    DeserializationError err = deserializeJson(db, http.getStream());
    if (!err) {
      JsonObject fr = db["response"]["flightroute"];
      if (!fr.isNull()) {
        JsonObject orig = fr["origin"];
        JsonObject dest = fr["destination"];
        String destCode = dest["iata_code"] | "";
        String origCode = orig["iata_code"] | "";
        
        JsonObject aptObj;
        if (baro_rate < -150) { // Clearly descending -> Destination
          aptObj = dest;
        } else if (baro_rate > 150) { // Clearly climbing -> Origin
          aptObj = orig;
        } else {
          // Neutral rate: pick the physically closer airport
          float dOrig = 9999, dDest = 9999;
          if (origCode.length()) dOrig = haversine(lat, lon, (float)orig["latitude"], (float)orig["longitude"]);
          if (destCode.length()) dDest = haversine(lat, lon, (float)dest["latitude"], (float)dest["longitude"]);
          
          if (dDest < dOrig && destCode.length()) aptObj = dest;
          else if (origCode.length()) aptObj = orig;
        }
        if (!aptObj.isNull()) {
          inferred_apt_code = aptObj["iata_code"]     | "";
          inferred_apt_name = aptObj["name"]          | "";
          inferred_apt_city = aptObj["municipality"] | "";
          inferred_apt_icao = aptObj["icao_code"]     | "";
          inferred_apt_lat  = aptObj["latitude"]      | acLat;
          inferred_apt_lon  = aptObj["longitude"]     | acLon;

          // Sanity check: if airport is > 500km away, it's likely a stale/wrong DB entry for the callsign
          float d_km = haversine(lat, lon, inferred_apt_lat, inferred_apt_lon);
          if (d_km > 500.0f) {
            Log.printf("Inference REJECTED: %s is %.1fkm away (> 500km)\n", inferred_apt_code.c_str(), d_km);
            inferred_apt_code = "---";
            inferred_apt_name = "";
            inferred_apt_city = "";
            inferred_apt_icao = "";
            inferred_apt_lat = 0;
            inferred_apt_lon = 0;
            success = false;
          } else {
            Log.printf("Inferred: %s (%s) at %.4f, %.4f\n", 
              inferred_apt_code.c_str(), inferred_apt_name.c_str(), 
              inferred_apt_lat, inferred_apt_lon);
            success = true;
          }
        }
      } else {
        Log.println("No flightroute in response");
      }
    } else {
      Log.printf("JSON parse failed for ADS-B DB: %s\n", err.c_str());
    }
  }
  http.end();
  lastInference = millis();
  if (inferred_apt_code == "---" || inferred_apt_code == "") inferred_apt_code = "APT";
  return success;
}
