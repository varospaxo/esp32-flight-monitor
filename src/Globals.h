#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include "TelnetLog.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ─── Mutexes ──────────────────────────────────────────────────────────────────
extern SemaphoreHandle_t configMutex;
extern SemaphoreHandle_t previewMutex;

// ─── Logger ───────────────────────────────────────────────────────────────────
extern TelnetLogger Log;

// ─── Runtime flags ────────────────────────────────────────────────────────────
extern volatile bool adsbOk;
extern volatile bool weatherOk;
extern volatile bool updating;
extern volatile bool savePending;

// ─── Shared state ─────────────────────────────────────────────────────────────
extern String preview;
extern unsigned long lastUpdate;
extern unsigned long lastSuccess;
extern unsigned long lastInference;

#endif // GLOBALS_H
