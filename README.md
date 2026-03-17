# ESP32 Flight Monitor

A modular ESP32-based flight radar and weather monitor. This project displays real-time aircraft data, airport arrivals, radar maps, weather information, and more on a TFT display.

## Features

- **Mode 1: Flight Radar**: Displays nearby aircraft with airline logos and route information.
- **Mode 2: Airport Monitor**: Shows arrivals for the nearest inferred airport.
- **Mode 3: Radar Map**: Visualizes aircraft positions on a 2D radar screen.
- **Mode 4: Weather + AQI**: Real-time weather data and Air Quality Index from Open-Meteo.
- **Mode 5: NTP Clock**: Synchronized clock with configurable timezones.
- **Mode 6: System Monitor**: WiFi status, API health, and system heap information.

## Hardware Requirements

- ESP32 (e.g., ESP32-WROOM-32)
- TFT Display (compatible with TFT_eSPI, e.g., ILI9341 320x240)
- Internet connection via WiFi

## Software Infrastructure

- **Asynchronous Web Server**: Configuration dashboard for settings and WiFi.
- **Modular Codebase**: Separated into functional modules for easy maintainability.
- **Persistent Storage**: LittleFS for configuration and web assets.

## Setup Instructions

1.  **WiFi Configuration**: On the first boot, the ESP32 will start an Access Point named `ESP32-Radar`. Connect to it and navigate to `192.168.4.1` to configure your WiFi credentials.
2.  **Dashboard**: Once connected to WiFi, access the web dashboard via the ESP32's IP address. Default credentials: `admin` / `admin`.
3.  **Location**: Set your latitude, longitude, and range in the dashboard to get accurate local data.

## Libraries Used

- `TFT_eSPI`
- `ArduinoJson`
- `ESPAsyncWebServer`
- `AsyncTCP`
- `LittleFS`
- `TJpg_Decoder`
- `ElegantOTA`

## License

MIT License
