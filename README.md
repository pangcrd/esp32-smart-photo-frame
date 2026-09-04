# PhotoFrame

[![ESP32](https://img.shields.io/badge/ESP32-E7352C?logo=espressif&logoColor=white)](https://www.espressif.com/en/products/socs/esp32)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-FF6C37?logo=platformio&logoColor=white)](https://platformio.org/)
[![VS Code](https://img.shields.io/badge/VS%20Code-007ACC?logo=visualstudiocode&logoColor=white)](https://code.visualstudio.com/)
[![Arduino Framework](https://img.shields.io/badge/Framework-Arduino-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![Open Source](https://img.shields.io/badge/Open--Source-333333?logo=github&logoColor=white)](https://github.com/pangcrd)
[![YouTube](https://img.shields.io/badge/YouTube-FF0000?logo=youtube&logoColor=white)](https://www.youtube.com/@pangcrd)
[![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-FFDD00?logo=buymeacoffee&logoColor=black)](https://www.buymeacoffee.com/pangcrd)

An ESP32-powered smart photo frame that displays images from an SD card and provides a web dashboard for remote management.

## Features

- Automatic photo slideshow from an SD card
- Responsive web dashboard for phones, tablets, and computers
- WiFi scanning, setup, reset, and status monitoring
- Image upload, crop, rotate, zoom, and deletion
- JPG/JPEG support with an optimized `320 x 240` display size
- Adjustable slideshow interval from 1 to 3600 seconds
- Weather information by latitude, longitude, timezone, and city
- CPU temperature, free RAM, WiFi signal, uptime, IP address, and SD card status
- English and Vietnamese interface
- Light/dark themes and customizable accent colors
- Device restart and factory reset options

## Quick Start

1. Power on the PhotoFrame.
2. Connect your phone or computer to the same WiFi network.
3. Open the device IP address in a browser, or try `photoframe.local` when mDNS is available.
4. Open **Gallery** to upload photos.
5. Open **Weather** to configure a location.

If WiFi is not configured, the PhotoFrame creates a setup network. Connect to it and follow the browser instructions.

## Photo Requirements

- Formats: `.jpg` and `.jpeg`
- Recommended size: `320 x 240` pixels
- Use **Edit Mode** to crop, reposition, zoom, or rotate images with other dimensions.

Keep at least 50 MB free on the SD card. Do not remove the card during uploads or deletions.

## Dashboard

The dashboard reports device health and storage status, including CPU temperature, free RAM, WiFi signal strength, SD card availability, uptime, IP address, and storage usage. Display brightness can be adjusted directly from the dashboard.

## Weather Setup

In **Weather**, enter:

- Latitude
- Longitude
- GMT timezone, such as `+07` or `-05`
- City name

An Internet connection is required to update weather data.

## Settings

Use **Settings** to change the theme and accent color, restart the device, or restore factory settings. Factory reset removes WiFi, weather, gallery, and slideshow settings, but does not delete photos stored on the SD card.

## Troubleshooting

- **Dashboard unavailable:** Verify power, WiFi connection, device IP, and `photoframe.local` support.
- **Photos missing:** Check the SD card, file extension, and available storage.
- **Upload failed:** Use JPG/JPEG, confirm `320 x 240` dimensions, or enable Edit Mode.
- **Weather not updating:** Check Internet access and location values.
