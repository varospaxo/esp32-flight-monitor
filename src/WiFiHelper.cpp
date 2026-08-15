#include "WiFiHelper.h"
#include "Config.h"
#include "Globals.h"

bool isAPMode = false;
DNSServer dnsServer;
const byte DNS_PORT = 53;

void startAP() {
  isAPMode = true;
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-Radar");
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(DNS_PORT, "*", apIP);
  Log.printf("AP: ESP32-Radar | IP: %s | DNS Captive Portal started\n", apIP.toString().c_str());
}

void handleDNS() {
  if (isAPMode) {
    dnsServer.processNextRequest();
  }
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
  WiFi.mode(WIFI_STA);
  WiFi.begin(c_ssid.c_str(), c_pass.c_str());
  Log.print("Connecting");
  int t = 20;
  while (WiFi.status() != WL_CONNECTED && t--) {
    delay(500);
    Log.print(".");
  }
  Log.println();
  if (WiFi.status() != WL_CONNECTED) {
    startAP();
  } else {
    isAPMode = false;
    Log.println("Connected: " + WiFi.localIP().toString());
  }
}
