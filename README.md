[![GitHub Tag](https://img.shields.io/github/v/tag/lukeswitz/AtomGPS_wigler?label=release)](https://github.com/lukeswitz/AtomGPS_wigler/releases)

# AtomGPS Wigler

## Overview
**AtomGPS Wigler** is a wardriving tool originally created by [@lozaning](https://github.com/lozaning). For use with the M5Stack Atom GPS kit, this tool is specifically designed for Wi-Fi network geolocation. LED status indicators are outlined below. [Wigle](wigle.net) compatible CSV files are written to SD.

### Prerequisites

- M5 AtomGPS
- SD card (Formatted FAT32)
- Arduino IDE or ` Esptool.py`


## Flashing to AtomGPS
- **Download this forked codebase:**
    ```bash
    git clone https://github.com/lukeswitz/AtomGPS_wigler.git
    cd AtomGPS_wigler/build
    ```

### Method One: [Esptool.py](https://docs.espressif.com/projects/esptool/en/latest/esp32/)

**1. Locate the device:**
-  Linux
     ```
     ls /dev/ttyUSB*
     ```
 - macOS:

   ```
   ls /dev/cu.*
    ```
   
   - Windows, check COM port in Device Manager.

**2. Flash the firmware, partition and bootloader:**

> [!WARNING]
> Ensure you have the latest version of esptool.py installed from the link above, or a known warning about header fields will display when flashing.

  - Navigate to the build folder (if not already there).   
  - Flash using the following command **inside the build folder or by specifying file paths:**

`esptool.py -p [YOUR_PORT] -b 1500000 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_size detect --flash_freq 80m 0x1000 AtomGPS_wigler_v1_5.bootloader.bin 0x8000 AtomGPS_wigler_v1_5.partitions.bin 0x10000 AtomGPS_wigler_v1_5.bin`

---

### Method 2: [Arduino IDE](https://www.arduino.cc/en/software)

**1. Open **the .ino file** in Arduino IDE (or copy and paste it into a new sketch).**

**2. Add & Install ESP32 Boards:**
   
   -  Open Arduino IDE.
   -  Go to `File > Preferences`.
   -  Add `https://dl.espressif.com/dl/package_esp32_index.json` to "Additional Boards Manager URLs."
   -  Click `OK`.
   -  Navigate to `Tools > Board > Boards Manager`.
   -  Search for "esp32" and search for "esp32 by Espressif Systems" and  click `Install`
 
   <img width="213" alt="image" src="https://github.com/lukeswitz/AtomGPS_wigler/assets/10099969/8b1c22f6-5721-4fad-b9e6-9464a8fe70e2">


**3. Add the Required Libraries:**
```
M5Atom
SD
SPI
TinyGPSPlus
WiFi
```
For example:
   - `Library Manager` > search `M5Atom`. Click `Install`. 

   <img width="198" alt="image" src="https://github.com/lukeswitz/AtomGPS_wigler/assets/10099969/949ed242-9b43-44ed-a2fe-160cadb20d3d">
    

**4. Set Board: Tools > Board > esp32 > M5Atom**

**5. The default settings are as below:**

<img width="425" alt="image" src="https://github.com/lukeswitz/AtomGPS_wigler/assets/10099969/c9a7ffc9-69f1-44ad-92a2-acf64e64c0bf">

**6. Click Upload**

## Get out Wardriving!

After flashing, the device scans for Wi-Fi networks, using LEDs to display status. Plug it into any usb power source and go:
**LED Indicators:**
> [!NOTE]  
>- Press and hold the button during scanning to toggle the Green LED.
- RED blink if the SD card is missing/write error.
- PURPLE blink while waiting for a GPS fix.
- GREEN blink during scanning (toggle by press & hold of button).
    
- Network data is saved in Wigle.net-compatible CSV files on the SD card. 
- ***Bonus: Contribute to the [wigle.net](https://wigle.net) database. Or join the community and compete to find the most Wi-Fi networks.***

### Feedback & Community

Detailed review and instructions for beginners | *Thanks to kampf for the writeup:*
- January 11, 2024: *["Wigle Me This"](https://zzq.org/?p=221)* by zZq.org

---
> [!CAUTION]
> Wardriving may not be legal in all locations. Please check local laws and obtain proper consent where necessary.
