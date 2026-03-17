#include "WiFiHelper.h"
#include "Config.h"
#include "Globals.h"

void startAP() {
  WiFi.softAP("ESP32-Radar");
  Serial.println("AP: ESP32-Radar");
}

void connectWiFi() {
  String c_ssid, c_pass;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_ssid = ssid;
  c_pass = pass;
  xSemaphoreGive(configMutex);

  if (c_ssid.length() == 0) {
    startAP();
    return;
  }
  
  WiFi.begin(c_ssid.c_str(), c_pass.c_str());
  Serial.print("Connecting");
  int t = 20;
  while (WiFi.status() != WL_CONNECTED && t--) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() != WL_CONNECTED) {
    startAP();
  } else {
    Serial.println("Connected: " + WiFi.localIP().toString());
  }
}
