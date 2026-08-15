#ifndef WIFI_HELPER_H
#define WIFI_HELPER_H
#include <WiFi.h>
#include <DNSServer.h>

extern bool isAPMode;
extern DNSServer dnsServer;

void startAP();
void connectWiFi();
void handleDNS();

#endif // WIFI_HELPER_H
