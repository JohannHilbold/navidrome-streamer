# Navidrome Streamer

A standalone music player for the [Waveshare ESP32-S3 Knob Touch LCD 1.8"](https://www.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8) that streams from a [Navidrome](https://www.navidrome.org/) server via the Subsonic API.

## Features

- Browse your music library: Favorites, Artists, Playlists
- Album art display on the 360×360 round AMOLED
- Play queue with auto-advance and wrap-around
- Touch gestures and rotary encoder navigation
- WiFi + Navidrome credentials configured via captive portal (no hardcoding)
- Multiple saved WiFi networks (connects to the strongest available)

## Hardware

- **Board**: Waveshare ESP32-S3 Knob Touch LCD 1.8" (ESP32-S3R8 + 8MB PSRAM + 16MB Flash)
- **Display**: 360×360 round AMOLED (SH8601 QSPI driver)
- **Touch**: CST816D capacitive touch (I2C)
- **Audio**: PCM5100A DAC via I2S
- **Input**: Rotary encoder + capacitive touchscreen

## Controls

| Context | Swipe Up | Swipe Down | Swipe Left | Swipe Right | Tap | Knob |
|---|---|---|---|---|---|---|
| **Menu** | Back | Select | — | — | Select | Scroll |
| **Now Playing** | Back to menu | — | Prev song | Next song | Play/Pause | Volume |

## First-Time Setup

1. Flash the firmware (see below)
2. The device starts an open WiFi access point called **ESP32-Music**
3. Connect to it with your phone
4. Open a browser — the setup page loads automatically
5. Add your WiFi network(s) and enter your Navidrome server URL, username, and password
6. Tap **Save & Reboot**

The device connects to WiFi and shows the main menu. Credentials are saved in flash and persist across reboots.

To reconfigure: navigate to **Settings → WiFi Setup** on the device, or type `setup` in the serial monitor.

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Upload (enter bootloader first — see below)
pio run --target upload

# Serial monitor
pio device monitor
```

### Entering Bootloader Mode

1. Hold the pinhole button next to the USB-C port
2. Power off, then power on while still holding
3. COM4 appears — flash to this port
4. After first flash, type `flash` in the serial monitor to re-enter bootloader without the pinhole

### USB Cable Note

The board has a dual-chip USB switch controlled by cable orientation. Use a **USB-A to USB-C** cable (not USB-C to USB-C). One orientation gives COM6 (serial monitor), the other gives COM5 (wrong chip). The bootloader appears on COM4.

## Project Structure

```
src/
  main.cpp        — Boot flow, WiFi, serial commands, setup/loop
  config.h        — Pin definitions, colors, layout constants
  api.h/cpp       — Subsonic API auth and browsing endpoints
  display.h/cpp   — AMOLED rendering, text, menu lists, cover art
  input.h/cpp     — Touch controller + rotary encoder
  player.h/cpp    — Audio playback, play queue, volume
  ui.h/cpp        — Screen state machine, navigation, gesture handling
  settings.h/cpp  — NVS credential storage (WiFi networks + Navidrome)
  portal.h/cpp    — Captive portal for first-time configuration
  font5x7.h       — Bitmap font data
  display/        — SH8601 panel driver (C)
```

## Serial Commands

Available as a debug/fallback interface:

| Command | Action |
|---|---|
| `next` / `prev` | Next/previous song in queue |
| `pause` | Toggle play/pause |
| `stop` | Stop playback |
| `v+` / `v-` | Volume up/down |
| `ping` | Test Navidrome connection |
| `setup` | Start WiFi captive portal |
| `reset` | Clear all saved settings and reboot |
| `status` | Show volume, WiFi, playback, memory info |
| `flash` | Reboot into bootloader for reflashing |

## Dependencies

All managed by PlatformIO (`lib_deps` in `platformio.ini`):

- [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) — Audio streaming and decoding
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) — Subsonic API response parsing
- [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder) — Album art JPEG decoding
- [CST816S](https://github.com/fbiego/CST816S) — Touch controller driver

Built-in ESP32 libraries: WiFi, WiFiMulti, HTTPClient, WebServer, DNSServer, Preferences.
