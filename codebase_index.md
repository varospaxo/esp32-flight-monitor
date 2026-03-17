# 🗂️ ESP32 Flight Monitor: Codebase Index

This document serves as a high-level guide to the repository, providing a roadmap for the system's architecture, core modules, and data flow.

---

## 🏗️ Architecture Overview

The ESP32 Flight Monitor is a real-time tracking application that fetches flight and weather data via external APIs and displays it on a 2.8" TFT screen (ILI9341). It also hosts an asynchronous web server for remote monitoring and configuration.

### Core Technologies
- **Framework**: Arduino / ESP-IDF
- **UI**: TFT_eSPI (Display), TJpg_Decoder (Logos/Flags)
- **Networking**: ESPAsyncWebServer, HTTPClient (API requests)
- **Storage**: LittleFS (JSON configuration)

---

## 📂 Directory Structure

### `src/` (Core Logic)
| File | Purpose | Key Responsibilities |
| :--- | :--- | :--- |
| [`main.cpp`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/src/main.cpp) | Entry Point | Setup, main loop, hardware button polling, mode switching. |
| [`Globals.h`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/src/Globals.h) | Global State | Mutexes (`configMutex`, `previewMutex`), runtime flags, and shared strings. |
| [`Config.cpp/.h`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/src/Config.h) | Configuration | Persistence using LittleFS, JSON loading/saving (Lat/Lon, Units, Filters). |
| [`Modes.cpp/.h`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/src/Modes.h) | Display Modes | Logic for Flight Radar, Airport Arrivals, Radar Map, Weather, and Clock. |
| [`Display.cpp/.h`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/src/Display.h) | TFT Helpers | Low-level drawing functions, header rendering, and JPEG callbacks. |
| [`FlightData.cpp/.h`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/src/FlightData.h) | Data Processing | `inferAirport` logic, fetching airport/airline metadata from external APIs. |
| [`WebServerHelper.cpp/.h`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/src/WebServerHelper.h) | Web Interface | REST API endpoints, static file serving, and dashboard authentication. |
| [`WiFiHelper.cpp/.h`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/src/WiFiHelper.h) | Connectivity | Connection management, AP fallback logic. |
| [`Utils.cpp/.h`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/src/Utils.h) | Helpers | Haversine distance calculations, heading arrows, AQI labels. |

### `data/` (Web Assets)
| File | Purpose |
| :--- | :--- |
| [`index.html`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/data/index.html) | Dashboard UI structure, including the **Live View** monitor and Settings panel. |
| [`script.js`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/data/script.js) | Frontend logic: TFT mirroring (Canvas), polling status, saving config. |
| [`style.css`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/data/style.css) | Dark-themed premium aesthetic for the dashboard. |
| [`config.json`](file:///c:/Users/vedan/Desktop/GitHub/esp32-flight-monitor/data/config.json) | Default/fallback configuration values. |

---

## 📡 External APIs & Dependencies

- **Flight Data**: [opendata.adsb.fi](https://opendata.adsb.fi) (ADS-B data)
- **Aircraft Metadata**: [adsbdb.com](https://api.adsbdb.com) (Callsign/Aircraft info)
- **Airlines**: [AirHex](https://airhex.com) (Airline logos)
- **Geography**: [FlagCDN](https://flagcdn.com) (Country flags)
- **Weather**: [Open-Meteo](https://open-meteo.com) (Forecast & Air Quality)
- **Time**: [NTP](http://pool.ntp.org) (Clock synchronization)

---

## 🔄 Key Data Flows

### 1. The Rendering Loop
`main.cpp` calls `updateMode()` every ~1s (Clock) or ~10s (Tracking). `updateMode()` executes the corresponding function in `Modes.cpp`, which fetches data via `HTTPClient`, clears the screen, and draws components.

### 2. State Mirroring (Live View)
When `Modes.cpp` finishes drawing, it calls `setPreview(String)`. This updates a global string protected by `previewMutex`. The web dashboard polls `/api/status`, receives this text, and displays it in the **Live View** tab.

### 3. Thread Safety
Since the Web Server (Core 0/Async) and the Main Loop (Core 1) both access configuration, all variables in `Config.h` MUST be wrapped in `xSemaphoreTake(configMutex, ...)` for thread safety.

---

## 🧠 Core Logic & Algorithms

### Airport Inference (`FlightData.cpp`)
The system can "guess" the nearest airport by observing local traffic.
- **Algorithm**: It scans all aircraft in range and picks the one with the **lowest altitude** (must be below 5000ft).
- **Metadata Search**: It takes that aircraft's callsign and queries `adsbdb.com`.
- **Directional Logic**: If the aircraft has a negative vertical speed (`baro_rate < 0`), it assumes the aircraft is landing and sets the inferred airport to the **Destination**. If positive, it assumes the **Origin**.
- **Persistence**: Inference results are cached and refreshed every 60 seconds to prevent API spam.

### Distance & Navigation (`Utils.cpp`)
- **Haversine Formula**: Used to calculate the distance between the user's home coordinates and aircraft or airports, accounting for the Earth's curvature.
- **Heading Indicators**: A custom function maps numerical 0-360 tracking data into 8 cardinal directions (N, NE, E, SE, S, SW, W, NW) with visual arrows.

### Display Mode Specifics (`Modes.cpp`)
- **Main Radar (`modeFlight`)**: Sorts aircraft by proximity. It handles unit conversion for altitude (ft to m) and distance (nm to km) on-the-fly based on user settings.
- **Radar Map (`modeMap`)**: Projects latitude/longitude offsets onto a 2D canvas. It uses a mathematical constraint to ensure aircraft dots stay within the radar rings.
- **Weather Analysis**: Combines current temperature, humidity, and wind speed with an AQI (Air Quality Index) fetching logic that color-codes the response (Green/Yellow/Red).

---

## ⚙️ Global Configuration Parameters
| Parameter | Type | Description |
| :--- | :--- | :--- |
| `lat`, `lon` | float | Home coordinates for distance calculations. |
| `units` | int | 0: Imperial, 1: Metric. |
| `f_ground`, `f_glider` | bool | Filtering flags for the radar view. |
| `mode` | int | Persistent display mode index (1-6). |
| `btnPin` | int | GPIO for the physical mode-switch button. |
