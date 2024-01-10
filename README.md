# AtomGPS Wigler

## Overview
AtomGPS_wigler is a wardriving tool **created by @lozaning** using the M5Stack Atom GPS kit. The goal is a lightweight and ultra-portable wardriver suitable for set-and-forget operation. 

Scans "learn" and auto-adjust to popular channels around you. Output is saved to Wigle.net compatible .CSV files.

## Prerequisites
- M5Stack Atom GPS Kit
- TF Card (4GB standard, tested up to 32GB)
- ESPtool or Arduino IDE

## Installation
Clone the repository:
```bash
git clone https://github.com/lukeswitz/AtomGPS_wigler.git
```

## Flashing Instructions

### Method 1: Arduino IDE
- Dependencies:
   - esp32 boards and the M5Atom library: [Docs](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- Open the .ino file in the IDE
- Set Board: esp32 > M5Stack-Core-ESP32
- Select your device port from the dropdown and press Upload/Flash
- _Note: Users have reported flashing using DIO instead of QIO and 40MHz instead of 80MHz resolving flashing problems._
---

### Method 2: Esptool
- Identify your device's port:
   - On Linux/macOS:
  ```bash
  ls /dev/ttyUSB*
  # or 
  ls /dev/cu.*
  ```
   - On Windows, check COM port in Device Manager.

- Flash the desired firmware .bin from buld folder:

 ```bash
   esptool.py --chip esp32 --port [PORT] --baud 115200 write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x10000 [FIRMWARE_FILE]
   ```

Replace `[PORT]` with your device's port and `[FIRMWARE_FILE]` with the firmware file.

## Usage
After flashing, the device starts scanning for Wi-Fi networks, indicating the status through LEDs and saving found networks to a Wigle.net compatible CSV file:  

- Purple Blink: Waiting for GPS fix.
- Green Blink: Scanning networks.
- Red Blink: SD card error.
- _Toggle the LED on/off during scans using the button (only during scanning will it register)_
