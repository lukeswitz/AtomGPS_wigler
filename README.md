# AtomGPS Wigler

## Overview
AtomGPS_wigler is a wardriving tool initially **created by @lozaning** using the M5Stack Atom GPS kit. It's designed for Wi-Fi network scanning with LED status indicators.

## Prerequisites
- M5Stack Atom GPS Kit (available at [DigiKey](https://www.digikey.com/en/products/detail/m5stack-technology-co-ltd/K043/13148796) and elsewhere)
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
      - esp32 boards (2.0.13) and the M5Atom (any) library. [Installation Docs](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html). 
- Open the .ino file in the IDE
- Set Board: esp32 > M5Stack-Atom
- Select your device port from the dropdown and press Upload/Flash

### Method 2: Esptool
- Identify your device's port:
   - On Linux/macOS:
  ```bash
  ls /dev/ttyUSB*
  # or 
  /dev/cu.*
  ```
   - On Windows, check COM port in Device Manager.

- Flash the firmware .bin version of your choice: 
```bash
esptool.py --chip auto --port [PORT] erase_flash write_flash -z 0x1000 [FIRMWARE_FILE]
```
Replace `[PORT]` with your device's port and `[FIRMWARE_FILE]` with the firmware file.

## Usage
After flashing, the device starts scanning for Wi-Fi networks, indicating the status through LEDs and saving found networks to a Wigle.net compatible CSV file. 
