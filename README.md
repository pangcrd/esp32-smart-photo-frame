# PhotoFrame

[![ESP32](https://img.shields.io/badge/ESP32-E7352C?logo=espressif&logoColor=white)](https://www.espressif.com/en/products/socs/esp32)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-FF6C37?logo=platformio&logoColor=white)](https://platformio.org/)
[![VS Code](https://img.shields.io/badge/VS%20Code-007ACC?logo=visualstudiocode&logoColor=white)](https://code.visualstudio.com/)
[![Arduino Framework](https://img.shields.io/badge/Framework-Arduino-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![Open Source](https://img.shields.io/badge/Open--Source-333333?logo=github&logoColor=white)](https://github.com/pangcrd)
[![YouTube](https://img.shields.io/badge/YouTube-FF0000?logo=youtube&logoColor=white)](https://www.youtube.com/@pangcrd)
[![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-FFDD00?logo=buymeacoffee&logoColor=black)](https://www.buymeacoffee.com/pangcrd)

An ESP32-powered smart photo frame that displays images from an SD card and provides a web dashboard for remote management.
[![demo](./images/preview.png)]
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

## Project Review

| Front | Left Side |
| --- | --- |
| [![Front view](./images/front_view.jpg)](./images/front_view.jpg) | [![Left side view](./images/left_view.jpg)](./images/left_view.jpg) |

| Right Side | Back Side |
| --- | --- |
| [![Right side view](./images/right_view.jpg)](./images/right_view.jpg) | [![Back side view](./images/back_view.jpg)](./images/back_view.jpg) |

## Webapp Preview

| Home Page | WiFi Page |
| --- | --- |
| [![Home page](./images/dashboard.png)](./images/dashboard.png) | [![WiFi page](./images/wifi_setting.png)](./images/wifi_setting.png) |

| Weather Page | Settings Page |
| --- | --- |
| [![Weather page](./images/weather_config.png)](./images/weather_config.png) | [![Settings page](./images/system_setting.png)](./images/system_setting.png) |

## Quick Start

1. Power on the PhotoFrame.
2. Connect your phone or computer to the same WiFi network.
3. Open the device IP address in a browser, or try `photoframe.local` when mDNS is available.
4. Open **Gallery** to upload photos.
5. Open **Weather** to configure a location.

If WiFi has not been configured, the PhotoFrame starts in **AP mode**:

1. Connect your phone or computer to the WiFi network `PhotoFrame-Setup`.
2. Open `http://192.168.4.1` in a browser.
3. Goto wifi page and configure your home WiFi network.
4. Reconnect your phone or computer to the newly configured WiFi network after the PhotoFrame restarts.

## Photo Requirements

- Formats: `.jpg` and `.jpeg`
- Recommended size: `320 x 240` pixels
- Use **Edit Mode** to crop, reposition, zoom, or rotate images with other dimensions.

Keep at least 20 MB free on the SD card. Do not remove the card during uploads or deletions.

## Weather Setup

In **Weather**, enter:
**You can get your "Latitude - Longitude" with Google map or Apple map**
- Latitude
- Longitude
- GMT timezone, such as `+07` or `-05`
- City name

An Internet connection is required to update weather data.

## Settings

Use **Settings** to change the theme and accent color, restart the device, or restore factory settings. Factory reset removes WiFi, weather, gallery, and slideshow settings, but does not delete photos stored on the SD card.

## JLCPCB Sponsor

If you are looking for high-quality PCB and stencil manufacturing, [JLCPCB](https://jlcpcb.com/coupon-center?from=pangcrd_coupon) is a great option and a familiar name among makers and engineers.

With advanced equipment and a professional manufacturing process, JLCPCB delivers reliable quality while keeping the process simple, from manufacturing to delivery at your doorstep.

JLCPCB also offers a PCB layout service for turning schematics into production-ready PCB files. Their engineers provide free reviews within three hours and follow an efficient workflow to help shorten your time to market.

To get started, visit the [JLCPCB ordering page](https://jlcpcb.com/promotion/1776849103?from=pangcrd_105coupon), upload your schematics and requirements, then choose the settings you need. Check the current layout promotion for a potential `$105` coupon.


## Recommended Parts

| Part | Suggested specification | Affiliate link |
| --- | --- | --- |
| Display | 2.8-inch LCD, 240 x 320 | [Buy LCD ST7789](https://s.click.aliexpress.com/e/_c2vXjECt) |
| Microcontroller | My ESP32-S3 custom board | [Only Shoppe]() |
| Custom your own | ESP32-S3 N16R8 | [Buy module](https://s.click.aliexpress.com/e/_c3ljzkZb) |
| Adafruit Micro SDcard| SDIO Card Breakout | [Buy module](https://s.click.aliexpress.com/e/_c45LSBBF) |
| Heat-set inserts M3 | M3*4*4.2 | [ Buy Brass Insert Nut](https://s.click.aliexpress.com/e/_c3gRMkTf) |
| Push button | Momentary 2pcs/latching 1pcs | [Buy button](https://s.click.aliexpress.com/e/_c3XV5Hi1) |
| Micro SD card | 1 GB or larger | [Buy SD Card](https://s.click.aliexpress.com/e/_c3R9ssnX) |

## Wiring diagram

[![demo](./images/wiring.png)]

## Firmware upload 
**This firmware version only use for ST7789 LCD driver**
**If you want use another LCD driver, you should build your own firmware with IDE**
[![demo](./images/how-to-install-firmware.png)]

## Troubleshooting

- **Dashboard unavailable:** Verify power, WiFi connection, device IP, and `photoframe.local` support.
- **Photos missing:** Check the SD card, file extension, and available storage.
- **Upload failed:** Use JPG/JPEG, confirm `320 x 240` dimensions, or enable Edit Mode.
- **Weather not updating:** Check Internet access and location values.
- **Non-commercial end-user**

