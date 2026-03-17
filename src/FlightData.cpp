#include "FlightData.h"
#include "Globals.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

float inferred_apt_lat = 0;
float inferred_apt_lon = 0;
String inferred_apt_code = "---";
String inferred_apt_name = "";
String inferred_apt_city = "";
String inferred_apt_icao = "";

void inferAirport(JsonArray ac) {
  if (inferred_apt_code != "---") {
    if (millis() - lastInference < 60000) return; // Wait 1 minute between refreshes
    // 1 minute passed, reset to re-infer from current nearby flights
    inferred_apt_code = "---"; inferred_apt_name = ""; inferred_apt_city = ""; inferred_apt_icao = "";
  }

  float best_alt = 99999;
  String best_flight = "";
  float best_lat = 0, best_lon = 0;
  int best_rate = 0;

  for (JsonObject a : ac) {
    if (!a["alt_baro"].is<int>()) continue;
    int alt = a["alt_baro"].as<int>();
    if (alt <= 0) continue;
    float acLat = a["lat"] | 0.0f;
    float acLon = a["lon"] | 0.0f;
    if (acLat == 0) continue;

    if (alt < best_alt && alt < 5000) {
      best_alt = alt;
      best_flight = a["flight"] | "";
      best_lat = acLat;
      best_lon = acLon;
      best_rate = a["baro_rate"] | 0;
    }
  }

  best_flight.trim();
  if (best_lat != 0) {
    inferred_apt_lat = best_lat;
    inferred_apt_lon = best_lon;
    if (best_flight.length() > 2) {
      Serial.printf("Inferring airport from flight: %s\n", best_flight.c_str());
      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;
      http.setTimeout(4000);
      String url = "https://api.adsbdb.com/v0/callsign/" + best_flight;
      http.begin(client, url);
      http.setUserAgent("Mozilla/5.0");
      int code = http.GET();
      Serial.printf("ADS-B DB Status: %d\n", code);
      if (code == 200) {
        String body = http.getString();
        Serial.printf("ADS-B DB Raw: %s\n", body.c_str());
        JsonDocument db;
        if (!deserializeJson(db, body)) {
          JsonObject fr = db["response"]["flightroute"];
          if (!fr.isNull()) {
            JsonObject orig = fr["origin"];
            JsonObject dest = fr["destination"];
            String destCode = dest["iata_code"] | "";
            String origCode = orig["iata_code"] | "";
            
            JsonObject aptObj;
            if (best_rate < 0 && destCode.length() > 0) aptObj = dest;
            else if (best_rate > 0 && origCode.length() > 0) aptObj = orig;
            else if (origCode.length() > 0) aptObj = orig;
            else if (destCode.length() > 0) aptObj = dest;
            
            if (!aptObj.isNull()) {
              inferred_apt_code = aptObj["iata_code"]     | "";
              inferred_apt_name = aptObj["name"]          | "";
              inferred_apt_city = aptObj["municipality"] | "";
              inferred_apt_icao = aptObj["icao_code"]     | "";
              inferred_apt_lat  = aptObj["latitude"]      | inferred_apt_lat;
              inferred_apt_lon  = aptObj["longitude"]     | inferred_apt_lon;
              Serial.printf("Inferred: %s (%s) at %.4f, %.4f\n", 
                inferred_apt_code.c_str(), inferred_apt_name.c_str(), 
                inferred_apt_lat, inferred_apt_lon);
            }
          } else {
            Serial.println("No flightroute key in response");
          }
        } else {
          Serial.println("JSON parse failed for ADS-B DB");
        }
      }
      http.end();
      lastInference = millis(); // Reset timer regardless of success to avoid spamming
    }
    if (inferred_apt_code == "---" || inferred_apt_code == "") inferred_apt_code = "APT";
  }
}
