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
unsigned long lastUpdate    = 0;
unsigned long lastModeCycle = 0;
unsigned long lastSuccess   = 0;
unsigned long lastInference = 0;
int gifDelayMs = 100;
volatile bool gifUploadPending = false;
// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  // Mutexes first -- everything else may touch shared state.
  configMutex  = xSemaphoreCreateMutex();
  previewMutex = xSemaphoreCreateMutex();
  if (!LittleFS.begin(true)) Log.println("LittleFS mount failed");
  loadConfig();
  connectWiFi();
  long c_offset;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_offset = tzOffset;
  xSemaphoreGive(configMutex);
  configTime(c_offset, 0, "pool.ntp.org", "time.nist.gov");
#if defined(TFT_BL) && defined(TFT_BACKLIGHT_ON)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif
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
  if (bPin >= 0 && bPin <= 39) pinMode(bPin, INPUT_PULLUP);
  else Log.printf("Invalid btnPin: %d, skipping pinMode\n", bPin);
  drawText("BOOTING...");
  setupServer();
  Log.startServer();
  String ipStr = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "AP: 192.168.4.1";
  drawText("READY\nIP " + ipStr + "\nMode " + String(mode));
  lastModeCycle = millis();
}
// ─── Loop (Core 1) ────────────────────────────────────────────────────────────
void loop() {
  ElegantOTA.loop();
  Log.handleClient();

  int c_mode;
  bool c_autoCycle;
  int c_cycleMins;
  String c_cycleModes;
  int c_btnPin;
  int c_customTextStyle;

  xSemaphoreTake(configMutex, portMAX_DELAY);
  c_mode = mode;
  c_autoCycle = autoCycle;
  c_cycleMins = cycleMins;
  c_cycleModes = cycleModes;
  c_btnPin = btnPin;
  c_customTextStyle = customTextStyle;
  xSemaphoreGive(configMutex);

  // 1. Hardware button press handling (debounce 400ms)
  static unsigned long lastBtnPress = 0;
  if (c_btnPin >= 0 && c_btnPin <= 39 && digitalRead(c_btnPin) == LOW && (millis() - lastBtnPress > 400)) {
    lastBtnPress = millis();
    int nextMode = getNextCycleMode(c_mode, c_cycleModes);
    xSemaphoreTake(configMutex, portMAX_DELAY);
    mode = nextMode;
    c_mode = nextMode;
    saveConfig();
    xSemaphoreGive(configMutex);
    lastModeCycle = millis();
    Log.printf("Button press: switched to mode %d\n", c_mode);
    updateMode();
    lastUpdate = millis();
  }

  // 1b. Touch swipe handling — poll in a tight loop to capture end position before finger lifts
  uint16_t tx, ty;
  if (tft.getTouch(&tx, &ty)) {
    TouchPoint touchStart = {tx, ty};
    TouchPoint touchEnd   = {tx, ty};
    unsigned long t0 = millis();

    // Keep sampling until finger lifts or 600ms timeout
    while (millis() - t0 < 600) {
      delay(8);
      if (!tft.getTouch(&tx, &ty)) break;
      touchEnd.x = tx;
      touchEnd.y = ty;
    }

    unsigned long dur = millis() - t0;
    Log.printf("Touch: (%d,%d)->(%d,%d) %lums\n",
               touchStart.x, touchStart.y, touchEnd.x, touchEnd.y, dur);

    SwipeDirection swipe = detectSwipe(touchStart, touchEnd);

    if (swipe == SWIPE_LEFT) {
      int nextMode = getNextCycleMode(c_mode, c_cycleModes);
      xSemaphoreTake(configMutex, portMAX_DELAY);
      mode = nextMode; c_mode = nextMode; saveConfig();
      xSemaphoreGive(configMutex);
      lastModeCycle = millis();
      Log.printf("Swipe left -> mode %d\n", c_mode);
      updateMode(); lastUpdate = millis();

    } else if (swipe == SWIPE_RIGHT) {
      // Find previous mode in cycle list
      int modeList[NUM_CYCLE_MODES]; int count = 0;
      for (int i = 0; i < NUM_CYCLE_MODES; i++) {
        int m = CYCLE_MODE_LIST[i];
        if (cycleModeEnabled(c_cycleModes, m)) modeList[count++] = m;
      }
      int prevMode = count > 0 ? modeList[count - 1] : c_mode; // default: wrap to last
      for (int i = 1; i < count; i++)
        if (modeList[i] == c_mode) { prevMode = modeList[i - 1]; break; }
      xSemaphoreTake(configMutex, portMAX_DELAY);
      mode = prevMode; c_mode = prevMode; saveConfig();
      xSemaphoreGive(configMutex);
      lastModeCycle = millis();
      Log.printf("Swipe right -> mode %d\n", c_mode);
      updateMode(); lastUpdate = millis();

    } else if (swipe == SWIPE_UP) {
      xSemaphoreTake(configMutex, portMAX_DELAY);
      mode = 8; c_mode = 8;
      xSemaphoreGive(configMutex);
      Log.println("Swipe up -> settings");
      updateMode(); lastUpdate = millis();

    } else if (swipe == SWIPE_DOWN) {
      xSemaphoreTake(configMutex, portMAX_DELAY);
      mode = 9; c_mode = 9;
      xSemaphoreGive(configMutex);
      Log.println("Swipe down -> network info");
      updateMode(); lastUpdate = millis();

    } else {
      // Tap — handle settings button presses
      if (c_mode == 8) {
        int action = handleSettingsTouch(touchStart.x, touchStart.y);
        if (action == 1) {
          xSemaphoreTake(configMutex, portMAX_DELAY);
          autoCycle = !autoCycle; saveConfig();
          xSemaphoreGive(configMutex);
          Log.printf("Auto-cycle -> %s\n", autoCycle ? "ON" : "OFF");
          updateMode(); lastUpdate = millis();
        } else if (action >= 100) {
          int modeNum = action - 100;
          xSemaphoreTake(configMutex, portMAX_DELAY);
          bool currentlyOn = cycleModeEnabled(cycleModes, modeNum);
          String newCycles = "";
          for (int i = 0; i < NUM_CYCLE_MODES; i++) {
            int m = CYCLE_MODE_LIST[i];
            bool keep = (m == modeNum) ? !currentlyOn : cycleModeEnabled(cycleModes, m);
            if (keep) {
              if (newCycles.length() > 0) newCycles += ",";
              newCycles += String(m);
            }
          }
          cycleModes = normalizeCycleModes(newCycles);
          saveConfig();
          xSemaphoreGive(configMutex);
          Log.printf("Mode %d toggled, cycles: %s\n", modeNum, cycleModes.c_str());
          updateMode(); lastUpdate = millis();
        }
      }
    }
  }

  // 2. Auto-Cycle Modes feature
  if (c_autoCycle) {
    if (lastModeCycle == 0) lastModeCycle = millis();
    unsigned long cycleIntervalMs = (unsigned long)max(1, c_cycleMins) * 60000UL;
    if (millis() - lastModeCycle >= cycleIntervalMs) {
      lastModeCycle = millis();
      int nextMode = getNextCycleMode(c_mode, c_cycleModes);
      xSemaphoreTake(configMutex, portMAX_DELAY);
      mode = nextMode;
      c_mode = nextMode;
      saveConfig();
      xSemaphoreGive(configMutex);
      Log.printf("Auto-cycle: switching to mode %d\n", c_mode);
      updateMode();
      lastUpdate = millis();
    }
  }

  // 3. Regular refresh for current mode
  unsigned long interval = (c_mode == 5) ? 1000UL : ((c_mode == 6) ? 15000UL : ((c_mode == 8 || c_mode == 9) ? 30000UL : 10000UL));
  if (c_mode == 7) interval = gifDelayMs;
  if (c_mode == 10 && c_customTextStyle == 1) interval = 60UL; // marquee needs frequent redraws to animate
  if (millis() - lastUpdate > interval) {
    if (c_mode != 7) Log.printf("Loop: trigger refresh for mode %d\n", c_mode);
    updateMode();
    lastUpdate = millis();
  }
}
