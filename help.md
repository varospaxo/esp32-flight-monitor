Here are the most commonly used PlatformIO CLI commands:

## Build & Upload
```bash
pio run                        # build
pio run -j 1                   # build single-threaded (fixes Windows ar.exe)
pio run -t upload              # build + upload firmware
pio run -t uploadfs            # upload data/ folder to LittleFS
pio run -t clean               # clean build cache
pio run -t erase               # erase entire flash
```

## Monitor
```bash
pio device monitor             # open serial monitor
pio device monitor -b 115200   # with baud rate
pio device monitor -p COM3     # specific port
pio run -t upload && pio device monitor   # upload then monitor
```

## Libraries
```bash
pio lib install "AuthorName/LibName"   # install library
pio lib list                           # list installed libs
pio lib update                         # update all libs
pio lib search "tft"                   # search registry
```

## Devices
```bash
pio device list                # list connected devices/ports
```

## Project
```bash
pio project init --board esp32dev    # create new project
pio run -e esp32                     # build specific env
```

## Useful Combos
```bash
# Clean + rebuild + upload + monitor in one line
pio run -t clean && pio run -j 1 -t upload && pio device monitor -b 115200

# Erase flash completely then re-upload everything
pio run -t erase && pio run -t uploadfs && pio run -t upload
```

The last combo under **Useful Combos** is your friend when the ESP32 is in a bad state.