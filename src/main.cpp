#include <Arduino.h>
#include <LittleFS.h>
#include <ElegantOTA.h>
#include <time.h>
#include <TJpg_Decoder.h>

#include "Globals.h"
#include "Config.h"
#include "Display.h"
#include "WiFiHelper.h"
#include "Utils.h"
#include "FlightData.h"
#include "Modes.h"
#include "WebServerHelper.h"

// ─── Globals Definition ───────────────────────────────────────────────────────
SemaphoreHandle_t configMutex  = nullptr;
SemaphoreHandle_t previewMutex = nullptr;

volatile bool adsbOk    = false;
volatile bool weatherOk = false;
volatile bool updating    = false;
volatile bool savePending = false;

String        preview     = "";
unsigned long lastUpdate  = 0;
unsigned long lastSuccess = 0;
unsigned long lastInference = 0;

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // Mutexes first -- everything else may touch shared state.
  configMutex  = xSemaphoreCreateMutex();
  previewMutex = xSemaphoreCreateMutex();

  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");

  loadConfig();
  connectWiFi();

  long c_offset;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_offset = tzOffset;
  xSemaphoreGive(configMutex);
  configTime(c_offset, 0, "pool.ntp.org", "time.nist.gov");

  tft.init();
  tft.setRotation(1);
  tftClear();
  
  // Setup jpeg decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true); // TFT_eSPI requires byte swapping
  TJpgDec.setCallback(tft_output);

  int bPin;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  bPin = btnPin;
  xSemaphoreGive(configMutex);
  if (bPin >= 0) pinMode(bPin, INPUT_PULLUP);

  drawText("BOOTING...");

  setupServer();

  String ipStr = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "AP: 192.168.4.1";
  drawText("READY\nIP " + ipStr + "\nMode " + String(mode));
}

// ─── Loop (Core 1) ────────────────────────────────────────────────────────────

void loop() {
  ElegantOTA.loop();

  int c_mode;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_mode = mode;
  xSemaphoreGive(configMutex);

  unsigned long interval = (c_mode == 5) ? 1000UL : 10000UL;

  if (millis() - lastUpdate > interval) {
    updateMode();
    lastUpdate = millis();
  }
}